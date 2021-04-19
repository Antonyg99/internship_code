#include "hw.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* 
 * This module is an intermediate step, until character device drivers are available, 
 * providing a C API to the FPGA-based AXI Stream FIFOs used to generate AXI Stream 
 * packets between the FPGA logic and Linux.  It works by memory-mapping in the 
 * AXI Stream FIFO's address region into this process's virtual address space so
 * we can perform pointer-based reads and writes of the AXI Stream FIFO's registers.
 */

#define AXIS_FIFO_BASE_ADDR  (0x43C00000)  // fixed physical address base of AXIS FIFO
#define AXIS_FIFO_RESET_KEY  (0x000000A5)  // value to write to reset registers

// structure defining the AXIS FIFO hardware's register interface
typedef volatile struct axis_fifo_s 
{
	uint32_t ISR;   // offset 0x00: interrupt status
	uint32_t IER;	// offset 0x04: interrupt enable 	
	uint32_t TDFR;	// offset 0x08: transmit data FIFO reset
	uint32_t TDFV;  // offset 0x0C: transmit data FIFO vacancy
	uint32_t TDFD;  // offset 0x10: transmit data FIFO write port
	uint32_t TLR;   // offset 0x14: transmit data length 
	uint32_t RDFR;  // offset 0x18: receive data FIFO reset
	uint32_t RDFO;	// offset 0x1C: receive data FIFO occupancy
	uint32_t RDFD;  // offset 0x20: receive data FIFO read port
	uint32_t RLR;   // offset 0x24: receive data length
	uint32_t SRR;   // offset 0x28: AXI4-Stream reset 
	uint32_t TDR;   // offset 0x2C: transmit destination 
	uint32_t RDR;   // offset 0x30: receive destination
} axis_fifo_t;

// pointer in this process's virtual memory space to AXIS FIFO hardware register interface
// (NULL until "hw_init()" is called
static axis_fifo_t *axis_fifo = NULL;

#define FIFO_ISR_RPURE (0x80000000)   // Receive packet length underrun read error (RLR read when empty)
#define FIFO_ISR_RPORE (0x40000000)   // Receive packet data overrun read error (RDFD read beyond current packet)
#define FIFO_ISR_RPUE  (0x20000000)   // Receive packet data underrun error (RDFD read when empty)
#define FIFO_ISR_TPOE  (0x10000000)   // Transmit packet data overrun error (TDFD written when FIFO full)
#define FIFO_ISR_TC    (0x08000000)   // Transmit complete (one or more packets transmitted)
#define FIFO_ISR_RC    (0x04000000)   // Receive complete (one or more packets received)
#define FIFO_ISR_TSE   (0x02000000)   // Transmit size error (FIFO words available < transmit words length)
#define FIFO_ISR_TRC   (0x01000000)   // Transmit reset complete (transmit logic reset - error recovery)
#define FIFO_ISR_RRC   (0x00800000)   // Receive reset complete (receive logic reset - error recovery)
#define FIFO_ISR_TFPF  (0x00400000)   // Transmit FIFO programmable full (TX FIFO full threshold crossed)
#define FIFO_ISR_TFPE  (0x00200000)   // Transmit FIFO programmable empty (TX FIFO empty threshold crossed)
#define FIFO_ISR_RFPF  (0x00100000)   // Receive FIFO programmable full (RX FIFO full threshold crossed)
#define FIFO_ISR_RFPE  (0x00080000)   // Receive FIFO programmable empty (RX FIFO empty threshold crossed)

#ifdef DEBUG

static void hw_debug_print_ISR(axis_fifo_t *fifo) 
{
	if( fifo != NULL )
	{
		uint32_t ISR = fifo->ISR;
		fprintf(stderr, "  ISR : %08X\t(interrupt status)\n", ISR);
		if( ISR & FIFO_ISR_RPURE )
			fprintf(stderr, "    RPURE: Receive packet length underrun read error.  RLR read when empty.\n");
		if( ISR & FIFO_ISR_RPORE )
			fprintf(stderr, "    RPORE: Receive packet data overrun read error.  RDFD read beyond current packet.\n");
		if( ISR & FIFO_ISR_RPUE )
			fprintf(stderr, "    RPUE : Receive packet data underrun error.  RDFD read when empty.\n");
		if( ISR & FIFO_ISR_TPOE )
			fprintf(stderr, "    TPOE : Transmit packet data overrun error.  TDFD written when FIFO full.\n");
		if( ISR & FIFO_ISR_TC )
			fprintf(stderr, "    TC   : Transmit packet complete.\n");
		if( ISR & FIFO_ISR_RC )
			fprintf(stderr, "    RC   : Receive packet complete.\n");
		if( ISR & FIFO_ISR_TSE )
			fprintf(stderr, "    TSE  : Transmit size error.  FIFO words available < requested transmit words length.\n");
		if( ISR & FIFO_ISR_TRC )
			fprintf(stderr, "    TRC  : Transmit reset complete. Transmit logic is reset.\n");
		if( ISR & FIFO_ISR_RRC )
			fprintf(stderr, "    RRC  : Receive reset complete.  Receive logic is reset.\n");
		if( ISR & FIFO_ISR_TFPF )
			fprintf(stderr, "    TFPF : Transmit FIFO programmble full.  TX FIFO full threshold crossed.\n");
		if( ISR & FIFO_ISR_TFPE )
			fprintf(stderr, "    TFPE : Transmit FIFO programmable empty.  TX FIFO empty threshold crossed.\n");
		if( ISR & FIFO_ISR_RFPF )
			fprintf(stderr, "    TFPF : Receive FIFO programmable full.  RX FIFO full threshold crossed.\n");
		if( ISR & FIFO_ISR_RFPE )
			fprintf(stderr, "    RFPE : Receive FIFO programmable empty.  RX FIFO empty threshold crossed.\n");
	}
}

// dump FIFO state.  NOTE: do not dump RDLR or RDFO as these are also FIFO-backed registers.  Reads empty them and corrupt HW state.
static void hw_debug_print_fifo_state(axis_fifo_t *fifo) 
{
	if( fifo == NULL )
		fprintf(stderr, "AXIS FIFO hardware not mapped.  Call hw_init()\n");
	else
	{
		fprintf(stderr, "AXIS FIFO STATE (vaddr: %p)\n", (void *)fifo);
		hw_debug_print_ISR(fifo);
		fprintf(stderr, "  IER : %08X\t(interrupt enable)\n", fifo->IER);
		fprintf(stderr, "  TDFV: %d\t(transmit data FIFO vacancy in 32-bit words)\n", fifo->TDFV);
		fprintf(stderr, "  TDR : %d\t(transmit destination)\n", fifo->TDR);
		fprintf(stderr, "  RDR : %d\t(receive destination)\n", fifo->RDR);
		fprintf(stderr, "\n");
	}
}
#endif /* DEBUG */

// reset the AXIS FIFO hardware state
static void hw_reset( axis_fifo_t *fifo ) 
{
	if( fifo == NULL )
		return;

	// reset the AXIS FIFO on each run
	fifo->SRR = AXIS_FIFO_RESET_KEY;
	// wait for transmit reset to complete
	while( !(fifo->ISR & FIFO_ISR_TRC) );
	// clear transmit complete flag and TFPF flag - seems to get set on reset
	fifo->ISR = (FIFO_ISR_TRC | FIFO_ISR_TFPF);
	// wait for receive reset to complete
	while( !(fifo->ISR & FIFO_ISR_RRC) );
	// clear receive reset complete flag and RFPF flag - seems to get set on reset
	fifo->ISR = (FIFO_ISR_RRC | FIFO_ISR_RFPF);

#ifdef DEBUG
	fprintf(stderr, "*****\nFIFO @ RESET:\n");
	hw_debug_print_fifo_state(axis_fifo);
#endif
}

// point *fifo at the AXIS FIFO hardware register space using mmap() on /dev/mem
static int hw_init( void ) 
{
	static int dmem_fd = 0;

	// is axis_fifo already mapped?
	if( axis_fifo != NULL )
	{
		fprintf(stderr, "ERROR: hw_init() attempted to map AXIS FIFO when already mapped.\n");
		return -1;
	}

	// is /dev/mem already open?
	if( dmem_fd != 0 )
	{
		fprintf(stderr, "ERROR: hw_init() attempted to open /dev/mem on non-zero file descriptor\n");
		return -1;
	}
	
	// open /dev/mem (RW)
	dmem_fd = open("/dev/mem", (O_RDWR | O_SYNC));
	if( dmem_fd <= 0 )
	{
		fprintf(stderr, "ERROR: hw_init() unable to open /dev/mem character device\n");
		return -1;
	}

	// figure out the starting physical address of the page containing the AXIS FIFO's hardware address
	unsigned int page_base_addr = AXIS_FIFO_BASE_ADDR & ~(off_t)(sysconf(_SC_PAGESIZE) -1);

	// figure out the FIFO hardware's address offset within a page boundary of an mmap()'d page
	unsigned int page_offset = AXIS_FIFO_BASE_ADDR & (sysconf(_SC_PAGESIZE) - 1);

	// get a pointer, in this process's virtual memory map space, to a page with the AXIS FIFO's 
	// physical address space mapped into it.  The AXIS FIFO's base address will start at the
	// offset_in_page address calculated above.
	void *mapped_page_vaddr = mmap(NULL,             // map to an arbitrary virtual address
			               sysconf(_SC_PAGESIZE),    // map a full page
			               (PROT_READ|PROT_WRITE),   // allow read, write operations
			               MAP_SHARED,               // sync this page map with any other mapped instances
			               dmem_fd, 		         // map the /dev/mem interface (physical memory as char device)
			               page_base_addr);		     // page boundary of page we want to map
	if( mapped_page_vaddr == MAP_FAILED )
	{
		fprintf(stderr, "ERROR: hw_init() unable to mmap() AXIS FIFO register space.\n");
		return -1;
	}

	// pointer magic: adjust *fifo to point at the physical FIFO hardware's register space
	axis_fifo = (axis_fifo_t *)(((char *)mapped_page_vaddr)+page_offset);

	// reset
	hw_reset(axis_fifo);

	return 0;
}


/*
 * Note: fd is ignored in this implementation as we do not yet have a dedicated
 * character device for the AXI FIFOs.  Instead we use an internally-managed
 * dmem_fd representing the generic /dev/mem physical memory interface 
 */
ssize_t hwread(int fd,void *buf, size_t count)
{
	uint32_t k; // generic iterator
	volatile uint32_t devnull; // used for dummy reads if flushing a packet from the RX FIFO

	// is the fifo mapped yet?  If not, map it
	if( axis_fifo == NULL )
		if( hw_init() )
			return -1;

	// is a packet available?
	if( !(axis_fifo->ISR & FIFO_ISR_RC) )
		return 0; // no data available

	// yes, check sizes and read the packet
	uint32_t length = axis_fifo->RLR;          // Length of packet data (in bytes)
	uint32_t word_reads = length / 4;          // Number of full 4-byte words to read
	if( length % 4 ) 						   // Increment word_reads if a partial read is required
		word_reads++;           
	
	// Enough space in buf to receive full packet?
	if( length > count )
	{
		fprintf(stderr, "ERROR hwread() received packet length (%d) exceeds receive buffer length (%d).  Dropping received packet.\n",
				length, count);
		// flush packet from RX FIFO
		for( k = 0; k < word_reads; k++ )
			devnull = axis_fifo->RDFD;
		// write to devnull to override compiler warning / build failure
		devnull = devnull;
		return -1;
	}

	// Copy packet into buffer - NOTE (TODO) we assume buffer is large enough to hold unused bytes from a partial FIFO word read
	for( k = 0; k < word_reads; k++ )
		((uint32_t *)buf)[k] = axis_fifo->RDFD;
	
	// Clear "packet received" flag 
	axis_fifo->ISR = FIFO_ISR_RC;

	// return number of bytes read
	return length;
}

// Blocking FIFO write - waits until transmit is complete before returning
ssize_t hwwrite(int fd,const void *buf, size_t count) 
{
	uint32_t k; // generic iterator

	// is the fifo mapped yet?  If not, map it
	if( axis_fifo == NULL )
		if( hw_init() )
			return -1;

	// yes, check there is sufficient space in the FIFO to write the packet
	uint32_t word_writes = count / 4;         // Number of full 4-byte words to write
	if( count % 4 ) 						  // Increment word_writes if a partial write is required
		word_writes++;           
	if( word_writes > axis_fifo->TDFV )
	{
		fprintf(stderr, "ERROR hwwrite() packet length (%d) exceeds transmit FIFO capacity.  Dropping.\n",
				count);
		return -1;
	}

	// Load the FIFO
	for( k = 0; k < word_writes; k++ )
		axis_fifo->TDFD = ((uint32_t *)buf)[k];

	// Send
	axis_fifo->TLR = count;

	// Wait for transmit to complete, then clear "transmit complete" flag
	while( !(axis_fifo->ISR & FIFO_ISR_TC) );
	axis_fifo->ISR = FIFO_ISR_TC;

	// return number of bytes written
	return count;
}