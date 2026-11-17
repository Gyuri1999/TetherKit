/* TetherKitRNDIS.h
 * Declaration of IOKit-derived classes for the TetherKit kernel extension
 * (the kext build of TetherKit, for injection by OpenCore).
 * Derived from HoRNDIS, a RNDIS driver for Mac OS X
 *
 *   Copyright (c) 2012 Joshua Wise.
 *   Copyright (c) 2018 Mikhail Iakhiaev
 *
 * IOKit examples from Apple's USBCDCEthernet.cpp; not much of that code remains.
 *
 * RNDIS logic is from linux/drivers/net/usb/rndis_host.c, which is:
 *
 *   Copyright (c) 2005 David Brownell.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <machine/limits.h>			/* UINT_MAX */
#include <libkern/OSByteOrder.h>
#include <libkern/OSTypes.h>

#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>

#include <IOKit/IOTimerEventSource.h>
#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOMessage.h>

#include <IOKit/usb/IOUSBHostFamily.h>
#include <IOKit/usb/IOUSBHostPipe.h>

extern "C"
{
	#include <sys/param.h>
	#include <sys/mbuf.h>
}

// Helps to avoid including private classes and methods into the symbol table.
#define NOEXPORT	__attribute__((visibility("hidden")))

// REFERENCES:
// [MS-RNDIS]: Remote Network Driver Interface Specification (RNDIS) Protocol
//   https://winprotocoldoc.blob.core.windows.net/productionwindowsarchives/WinArchive/[MS-RNDIS].pdf
// [MSDN-RNDISUSB]: Remote NDIS To USB Mapping
//   https://docs.microsoft.com/en-us/windows-hardware/drivers/network/remote-ndis-to-usb-mapping

// RNDIS structures, constants and buffer sizes are shared with the DriverKit
// driver, so both builds speak exactly the same protocol.
#include "../TetherKitDriver/RNDISProtocol.h"

#define TRANSMIT_QUEUE_SIZE     256

/***** Actual class definitions *****/

typedef struct {
	IOBufferMemoryDescriptor *mdp;
	IOUSBHostCompletion comp;
} pipebuf_t;

class TetherKitRNDIS : public IOEthernetController {
	OSDeclareDefaultStructors(TetherKitRNDIS);	// Constructor & Destructor stuff

private:
	/*!
	 * This class protects method calls against re-entry, when the IOCommand
	 * gate is being released due to synchronous IO, or some other reason.
	 * Use case:
	 * Unlike start/stop calls, that are triggered by a single IO provider,
	 * the enable/disable calls can be triggered by multiple interface clients,
	 * as well as user's actions, e.g. "ifconfig en6 up" - potentially multiple
	 * "ifconfig" processes running in parallel.
	 * Even though the calls to this class are protected by the IOCommandGate,
	 * synchronous USB transfers release the gate (by using 
	 * IOCommandGate::commandSleep), allowing another enable/disable call
	 * to "sneak in". We use the "ReentryLocker" to delay additional
	 * enable/disable calls until the first one completes.
	 */
	class NOEXPORT ReentryLocker {
	public:
		// 'inGuard' is instance-level variable that would be set by
		// "ReentryLocker" whenever someone is executing the protected section.
		ReentryLocker(IOCommandGate *inGate, bool &inGuard);
		ReentryLocker(IONetworkController *controller, bool &inGuard):
			ReentryLocker(controller->getCommandGate(), inGuard) {}
		~ReentryLocker();
		IOReturn getResult() const { return result; }
		bool isInterrupted() const { return result != kIOReturnSuccess; }
	private:
		IOCommandGate *const gate;
		bool &entryGuard;
		IOReturn result;
	};
		
	IOEthernetInterface *fNetworkInterface;
	IONetworkStats *fpNetStats;

	bool fReadyToTransfer;  // Ready to transmit: Android <-> MAC.
	// Set to true when 'enable' succeeds, and
	// set to false when 'disable' succeeds:
	bool fNetifEnabled;
	bool fEnableDisableInProgress;  // Guards against re-entry
	bool fDataDead;

	// These pass information from 'probe' to 'openUSBInterfaces':
	uint8_t fProbeConfigVal;
	uint8_t fProbeCommIfNum;  // The data interface number is +1.

	// fCallbackCount is the number of callbacks concurrently running
	// (possibly offset by a certain value).
	//  - Every successful async API call shall "fCallbackCount++".
	//  - Every time we exit the completion without making another call:
	//    'callbackExit()'.
	int fCallbackCount;
	
	// USB Communication:
	IOUSBHostInterface *fCommInterface;
	IOUSBHostInterface *fDataInterface;
	
	IOUSBHostPipe *fInPipe;
	IOUSBHostPipe *fOutPipe;
	
	uint32_t rndisXid;  // RNDIS request_id count.
	int32_t maxOutTransferSize;  // Set by 'rdisInit' from device reply.
	// wMaxPacketSize of the bulk OUT endpoint, for short-packet padding:
	uint16_t fOutPipeMaxPacketSize;

	pipebuf_t outbufs[N_OUT_BUFS];
	// Allow double-buffering to enable the best hardware utilization:
	pipebuf_t inbufs[N_IN_BUFS];
	uint16_t outbufStack[N_OUT_BUFS];
	int numFreeOutBufs;

	void callbackExit();
	static void dataWriteComplete(void *obj, void *param, IOReturn ior, UInt32 transferred);
	static void dataReadComplete(void *obj, void *param, IOReturn ior, UInt32 transferred);

	bool rndisInit();
	IOReturn rndisCommand(struct rndis_msg_hdr *buf, int buflen);
	int rndisQuery(void *buf, uint32_t oid, uint32_t in_len, void **reply, int *reply_len);
	bool rndisSetPacketFilter(uint32_t filter);

	IOService *probeDevice(IOUSBHostDevice *device, SInt32 *score);

	bool openUSBInterfaces(IOService *provider);
	void closeUSBInterfaces();
	void disableNetworkQueue();
	void disableImpl();

	bool createMediumTables(const IONetworkMedium **primary);
	bool allocateResources(void);
	void releaseResources(void);
	bool createNetworkInterface(void);

	void receivePacket(void *packet, UInt32 size);
	static void receiveFrame(void *context, const uint8_t *data, uint32_t length);

public:
	// IOKit overrides
	virtual bool init(OSDictionary *properties = 0) override;
	virtual void free() override;
	virtual IOService *probe(IOService *provider, SInt32 *score) override;
	virtual bool start(IOService *provider) override;
	virtual bool willTerminate(IOService *provider, IOOptionBits options) override;
	virtual void stop(IOService *provider) override;

	// IOEthernetController overrides
	virtual IOOutputQueue *createOutputQueue(void) override;
	virtual IOReturn getHardwareAddress(IOEthernetAddress *addr) override;
	virtual IOReturn getMaxPacketSize(UInt32 *maxSize) const override;
	virtual IOReturn getPacketFilters(const OSSymbol *group,
									  UInt32 *filters ) const  override;
	virtual IONetworkInterface *createInterface() override;
	virtual bool configureInterface(IONetworkInterface *netif) override;
	
	virtual IOReturn enable(IONetworkInterface *netif) override;
	virtual IOReturn disable(IONetworkInterface *netif) override;
	virtual IOReturn selectMedium(const IONetworkMedium *medium) override;
	virtual IOReturn setMulticastMode(bool active) override;
	virtual IOReturn setMulticastList(IOEthernetAddress *addrs,
	                                  UInt32             count) override;
	virtual IOReturn setPromiscuousMode(bool active) override;
	virtual UInt32 outputPacket(mbuf_t pkt, void *param) override;
};

class TetherKitRNDISInterface : public IOEthernetInterface {
	OSDeclareDefaultStructors(TetherKitRNDISInterface);
	int maxmtu;
public:
	virtual bool init(IONetworkController *controller, int mtu);
	virtual bool setMaxTransferUnit(UInt32 mtu) override;
};
