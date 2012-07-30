/****************************************************************
 NAME: usblib.c
 DESC: S3C2440X USB library functions
 HISTORY:
 Mar.25.2002:purnnamu: ported for S3C2410X.
 Mar.27.2002:purnnamu: DMA is enabled.
 ****************************************************************/
#include <common.h>
#include <asm/arch/s3c24x0_cpu.h>
#include <asm/io.h>

#include "2440usb.h"
#include "usbmain.h"
#include "usblib.h"
#include "usbsetup.h"
#include "usbmain.h"

#define BIT_USBD		(0x1<<25)

extern volatile U32 dwUSBBufReadPtr;
extern volatile U32 dwUSBBufWritePtr;
extern volatile U32 dwPreDMACnt;
extern volatile U32 dwNextDMACnt;



void ConfigUsbd(void)
{
	struct s3c24x0_interrupt * intregs = s3c24x0_get_base_interrupt();
	ReconfigUsbd();
	writel((readl(&intregs->intmsk) & ~(BIT_USBD)), &intregs->intmsk); 
}


void ReconfigUsbd(void)
{
// *** End point information ***
//   EP0: control
//   EP1: bulk in end point
//   EP2: not used
//   EP3: bulk out end point
//   EP4: not used
	struct s3c24x0_usb_device * const usbdevregs	= s3c24x0_get_base_usb_device();   
 
	writeb(PWR_REG_DEFAULT_VALUE, &usbdevregs->pwr_reg); //disable suspend mode
	writeb(0, &usbdevregs->index_reg); 
	writeb(FIFO_SIZE_8, &usbdevregs->maxp_reg); //EP0 max packit size = 8 
	writeb((EP0_SERVICED_OUT_PKT_RDY | EP0_SERVICED_SETUP_END), & usbdevregs->ep0_csr_in_csr1_reg); //EP0:clear OUT_PKT_RDY & SETUP_END
	writeb(1, &usbdevregs->index_reg); 

#if (EP1_PKT_SIZE==32)
	writeb(FIFO_SIZE_32, &usbdevregs->maxp_reg); //EP1:max packit size = 32
#else
	writeb(FIFO_SIZE_64, &usbdevregs->maxp_reg); 	//EP1:max packit size = 64
#endif	
	writeb((EPI_FIFO_FLUSH | EPI_CDT), &usbdevregs->ep0_csr_in_csr1_reg); 
	writeb((EPI_MODE_IN | EPI_IN_DMA_INT_MASK | EPI_BULK), &usbdevregs->in_csr2_reg);  //IN mode, IN_DMA_INT=masked    
	writeb(EPO_CDT, &usbdevregs->out_csr1_reg); 
	writeb((EPO_BULK | EPO_OUT_DMA_INT_MASK), &usbdevregs->out_csr2_reg); 

	writeb(2, &usbdevregs->index_reg); 
	writeb(FIFO_SIZE_64, &usbdevregs->maxp_reg); //EP2:max packit size = 64
	writeb((EPI_FIFO_FLUSH | EPI_CDT | EPI_BULK), &usbdevregs->ep0_csr_in_csr1_reg); 
	writeb((EPI_MODE_IN | EPI_IN_DMA_INT_MASK), &usbdevregs->in_csr2_reg);  //IN mode, IN_DMA_INT=masked    
	writeb(EPO_CDT, &usbdevregs->out_csr1_reg); 
	writeb((EPO_BULK | EPO_OUT_DMA_INT_MASK), &usbdevregs->out_csr2_reg); 

	writeb(3, &usbdevregs->index_reg); 
    #if (EP3_PKT_SIZE==32)
	writeb(FIFO_SIZE_32, &usbdevregs->maxp_reg); //EP3:max packit size = 32
    #else
	writeb(FIFO_SIZE_64, &usbdevregs->maxp_reg); //EP3:max packit size = 64
    #endif
	writeb((EPI_FIFO_FLUSH | EPI_CDT | EPI_BULK), &usbdevregs->ep0_csr_in_csr1_reg); 	
	writeb((EPI_MODE_OUT | EPI_IN_DMA_INT_MASK), &usbdevregs->in_csr2_reg); //OUT mode, IN_DMA_INT=masked    
	writeb(EPO_CDT, &usbdevregs->out_csr1_reg); 
    	//clear OUT_PKT_RDY, data_toggle_bit.
	//The data toggle bit should be cleared when initialization.
	writeb((EPO_BULK | EPO_OUT_DMA_INT_MASK), &usbdevregs->out_csr2_reg); 

	writeb(4, &usbdevregs->index_reg); 
	writeb(FIFO_SIZE_64, &usbdevregs->maxp_reg); //EP4:max packit size = 64
	writeb((EPI_FIFO_FLUSH | EPI_CDT | EPI_BULK), &usbdevregs->ep0_csr_in_csr1_reg); 
	writeb((EPI_MODE_OUT | EPI_IN_DMA_INT_MASK), &usbdevregs->in_csr2_reg); //OUT mode, IN_DMA_INT=masked    
	writeb(EPO_CDT, &usbdevregs->out_csr1_reg); 
    	//clear OUT_PKT_RDY, data_toggle_bit.
	//The data toggle bit should be cleared when initialization.
	writeb((EPO_BULK | EPO_OUT_DMA_INT_MASK), &usbdevregs->out_csr2_reg); 
    
	writeb((EP0_INT | EP1_INT | EP2_INT | EP3_INT | EP4_INT), &usbdevregs->ep_int_reg); 
	writeb((RESET_INT | SUSPEND_INT | RESUME_INT), &usbdevregs->usb_int_reg); 
    	//Clear all usbd pending bits
    	
	writeb((EP0_INT | EP1_INT | EP3_INT), &usbdevregs->ep_int_en_reg); 
	writeb(RESET_INT, &usbdevregs->usb_int_en_reg); 
    ep0State = EP0_STATE_INIT;
    
}


void RdPktEp0(U8 *buf,int num)
{
    int i;
	struct s3c24x0_usb_device * const usbdevregs	= s3c24x0_get_base_usb_device();   
 	
    for(i=0;i<num;i++)
    {
	 buf[i] = readb(&usbdevregs->fifo[0].ep_fifo_reg);
    }
}
    

void WrPktEp0(U8 *buf,int num)
{
    int i;
	struct s3c24x0_usb_device * const usbdevregs	= s3c24x0_get_base_usb_device();      	
    for(i=0;i<num;i++)
    {
	writeb(buf[i], &usbdevregs->fifo[0].ep_fifo_reg); 
    }
}


void WrPktEp1(U8 *buf,int num)
{
    int i;
	struct s3c24x0_usb_device * const usbdevregs	= s3c24x0_get_base_usb_device();      	
    for(i=0;i<num;i++)
    {
	writeb(buf[i], &usbdevregs->fifo[1].ep_fifo_reg); 
    }
}


void WrPktEp2(U8 *buf,int num)
{
    int i;
	struct s3c24x0_usb_device * const usbdevregs	= s3c24x0_get_base_usb_device();      	
    for(i=0;i<num;i++)
    {
	writeb(buf[i], &usbdevregs->fifo[2].ep_fifo_reg); 
    }
}


void RdPktEp3(U8 *buf,int num)
{
    int i;
	struct s3c24x0_usb_device * const usbdevregs	= s3c24x0_get_base_usb_device();      	
    for(i=0;i<num;i++)
    {
	 buf[i] = readb(&usbdevregs->fifo[3].ep_fifo_reg);
    }
}


void RdPktEp4(U8 *buf,int num)
{
    int i;
	struct s3c24x0_usb_device * const usbdevregs	= s3c24x0_get_base_usb_device();      	
    for(i=0;i<num;i++)
    {
	 buf[i] = readb(&usbdevregs->fifo[4].ep_fifo_reg);
    }
}


void ConfigEp3DmaMode(U32 bufAddr,U32 count)
{
	char j;
	int i;
	struct s3c24x0_usb_device * const usbdevregs = s3c24x0_get_base_usb_device();  
	struct s3c24x0_dmas * const dmaregs = s3c24x0_get_base_dmas();

	writeb(3, &usbdevregs->index_reg); 
	count &= 0xfffff; //transfer size should be <1MB
	writel(((1<<1) | (1<<0)), &dmaregs->dma[2].disrcc);     
	writel(ADDR_EP3_FIFO, &dmaregs->dma[2].disrc);  //src=APB,fixed,src=EP3_FIFO
	writel(((0<<1) | (0<<0)), &dmaregs->dma[2].didstc); 
	writel(bufAddr, &dmaregs->dma[2].didst); //dst=AHB,increase,dst=bufAddr
#if USBDMA_DEMAND
	writel(((count)|(0<<31)|(0<<30)|(1<<29)|(0<<28)|(0<<27)|(4<<24)|(1<<23)|(0<<22)|(0<<20)), &dmaregs->dma[2].dcon); 
        //demand,requestor=APB,CURR_TC int enable,unit transfer,
        //single service,src=USBD,H/W request,autoreload,byte,CURR_TC
#else
    /* changed by thisway.diy to disable autoreload */
	writel(((count)|(1<<31)|(0<<30)|(1<<29)|(0<<28)|(0<<27)|(4<<24)|(1<<23)|(1<<22)|(0<<20)), &dmaregs->dma[2].dcon); 
        //handshake,requestor=APB,CURR_TC int enable,unit transfer,
        //single service,src=USBD,H/W request,autoreload,byte,CURR_TC
#endif
	writel((1<<1), &dmaregs->dma[2].dmasktrig);         
        //DMA 2 on

    //rEP3_DMA_FIFO=0x40; //not needed for OUT operation. 	
	writeb(0xff, &usbdevregs->ep3.ep_dma_ttc_l); 
	writeb(0xff, &usbdevregs->ep3.ep_dma_ttc_m); 
	writeb(0x0f, &usbdevregs->ep3.ep_dma_ttc_h); 

	writeb((readb(&usbdevregs->out_csr2_reg) | EPO_AUTO_CLR | EPO_OUT_DMA_INT_MASK), &usbdevregs->out_csr2_reg); 
    	//AUTO_CLR(OUT_PKT_READY is cleared automatically), interrupt_masking.
#if USBDMA_DEMAND
	writeb(EP3_PKT_SIZE, &usbdevregs->ep3.ep_dma_ttc_h); 
	writeb((UDMA_DEMAND_MODE | UDMA_OUT_DMA_RUN | UDMA_DMA_MODE_EN), &usbdevregs->ep3.ep_dma_con); 
        // deamnd enable,out_dma_run=run,in_dma_run=stop,DMA mode enable
#else
	writeb(0x01, &usbdevregs->ep3.ep_dma_unit);         
	writeb((UDMA_OUT_DMA_RUN | UDMA_DMA_MODE_EN), &usbdevregs->ep3.ep_dma_con); 
        // deamnd disable,out_dma_run=run,in_dma_run=stop,DMA mode enable
#endif  
    //wait until DMA_CON is effective.
	j = readb(&usbdevregs->ep3.ep_dma_con);
	for(i=0;i<10;i++)
		asm volatile ("mov r0,r0");
    /* add by thisway.diy for non-autoreload */
	writel((1<<1), &dmaregs->dma[3].dmasktrig); 
}


void ConfigEp3IntMode(void)
{
	char i;	
	struct s3c24x0_usb_device * const usbdevregs = s3c24x0_get_base_usb_device(); 
	struct s3c24x0_dmas * const dmaregs = s3c24x0_get_base_dmas(); 

	writeb(3, &usbdevregs->index_reg); 
 	writel((0<<1), &dmaregs->dma[2].dmasktrig);    

        //DMA channel off
	writeb((readb(&usbdevregs->out_csr2_reg) & ~(EPO_AUTO_CLR)), &usbdevregs->out_csr2_reg); 
    	//AUTOCLEAR off,interrupt_enabled (???)
	writeb(1, &usbdevregs->ep3.ep_dma_unit); 	
	writeb(0, &usbdevregs->ep3.ep_dma_con); 
    	// deamnd disable,out_dma_run=stop,in_dma_run=stop,DMA mode disable
	//wait until DMA_CON is effective.
	i = readb(&usbdevregs->ep3.ep_dma_con);
    
}
