//Coto: these are my FIFO handling libs. Works fine with NIFI (trust me this is very tricky to do without falling into freezes).
//Use it at your will, just make sure you read WELL the descriptions below.

#include <nds.h>
#include "common_shared.h"

#ifdef ARM7

#include <string.h>

#include "wifi_arm7.h"
#include "arm7.h"
#include "audiosys.h"
#include "handler.h"
#include "c_defs.h"
#include "ds_misc.h"

#endif

#ifdef ARM9

#include "c_defs.h"
#include "ds_misc.h"

#endif
//Software FIFO calls, Rely on Hardware FIFO calls so it doesnt matter if they are in different maps 
#ifdef ARM9
__attribute__((section(".dtcm")))
#endif    
volatile int FIFO_SOFT_PTR = 0;
#ifdef ARM9
__attribute__((section(".dtcm")))
#endif    
volatile u32 FIFO_BUF_SOFT[FIFO_NDS_HW_SIZE/4];

//GetSoftFIFO: Stores up to FIFO_NDS_HW_SIZE. Exposed to usercode for fetching 64 bytes sent from other core, until it returns false (empty buffer).

//Example: 
//u32 n = 0;
//while(GetSoftFIFO(&n)== true){
//	//n has 4 bytes from the other ARM Core.
//}
#ifdef ARM9
__attribute__((section(".itcm")))
#endif    
inline bool GetSoftFIFO(u32 * var)
{
	if(FIFO_SOFT_PTR > 0){
		FIFO_SOFT_PTR--;
		
		*var = (u32)FIFO_BUF_SOFT[FIFO_SOFT_PTR];
		FIFO_BUF_SOFT[FIFO_SOFT_PTR] = (u32)0;
		
		return true;
	}
	else
		return false;
}

//SetSoftFIFO == false means FULL
#ifdef ARM9
__attribute__((section(".itcm")))
#endif    
//returns ammount of inserted U32 blocks into FIFO hardware regs
inline bool SetSoftFIFO(u32 value)
{
	if(FIFO_SOFT_PTR < (int)(FIFO_NDS_HW_SIZE/4)){
		FIFO_BUF_SOFT[FIFO_SOFT_PTR] = value;
		FIFO_SOFT_PTR++;
		return true;
	}
	else
		return false;
}

//SendArm[7/9]Command: These send a command and up to 15 arguments. 
//The other ARM Core through a FIFO interrupt will execute HandleFifo()
//By default I use 4 (you can fill them with 0s if you want to use fewer)
#ifdef ARM9
__attribute__((section(".itcm")))
inline void SendArm7Command(u32 command1, u32 command2, u32 command3, u32 command4)
#endif
#ifdef ARM7
inline void SendArm9Command(u32 command1, u32 command2, u32 command3, u32 command4)
#endif    
{	
	FIFO_DRAINWRITE();
	
	REG_IPC_FIFO_TX = command1;
	REG_IPC_FIFO_TX = command2;
	REG_IPC_FIFO_TX = command3;
	REG_IPC_FIFO_TX = command4;
	
	//always send full fifo queue
	REG_IPC_FIFO_CR |= IPC_FIFO_ERROR;
	
}

//FIFO HANDLER INIT
#ifdef ARM9
__attribute__((section(".itcm")))
#endif
inline void HandleFifoEmpty(){
	
}

//FIFO HANDLER INIT
#ifdef ARM9
__attribute__((section(".itcm")))
#endif
inline void HandleFifoNotEmpty(){
	u32 command1 = 0,command2 = 0,command3 = 0,command4 = 0;
	
	if(!(REG_IPC_FIFO_CR & IPC_FIFO_RECV_EMPTY)){
		command1 = REG_IPC_FIFO_RX;
	}
	
	if(!(REG_IPC_FIFO_CR & IPC_FIFO_RECV_EMPTY)){
		command2 = REG_IPC_FIFO_RX;
	}
	
	if(!(REG_IPC_FIFO_CR & IPC_FIFO_RECV_EMPTY)){
		command3 = REG_IPC_FIFO_RX;
	}
	
	if(!(REG_IPC_FIFO_CR & IPC_FIFO_RECV_EMPTY)){
		command4 = REG_IPC_FIFO_RX;
	}
	
	
	//ARM7 command handler
	#ifdef ARM7
	switch (command1) {
        
		case FIFO_APU_PAUSE:
			APU_paused=1;
			memset((u32*)buffer,0,sizeof(buffer));
			break;
		case FIFO_UNPAUSE:
			APU_paused=0;
			break;
		case FIFO_APU_RESET:
			memset((u32*)buffer,0,sizeof(buffer));
			APU_paused=0;
			resetAPU();
			break;
		case FIFO_SOUND_RESET:
			lidinterrupt();
			break;
	
        //arm9 wants to WifiSync
		case(WIFI_SYNC):{
			Wifi_Sync();
		}
        break;
        
		//must be called from within timer irqs
		//update apu from nds irq
		case(FIFO_APU_WRITE16):{
			
			//method 1
			//stack to fifo here
			SetSoftFIFO(command2);
			
			//method 2
			//u16 msg = (u16)(command2&0xffff);
			//APUSoundWrite(msg >> 8, msg&0xFF);
			//IPC_APUR = IPC_APUW;			
		
		}
		break;
		
		//arm9 wants to send a WIFI context block address / userdata is always zero here
        case(0xc1710101):{
            //	wifiAddressHandler( void * address, void * userdata )
            wifiAddressHandler((Wifi_MainStruct *)(u32)command2, 0);
        }
        break;
		
	}
	#endif
	
	//ARM9 command handler
	#ifdef ARM9
	switch (command1) {   
		case(WIFI_SYNC):{
			Wifi_Sync();
		}
		break;
    }
	#endif
	
	//Shared: Acknowledge
	REG_IPC_FIFO_CR |= IPC_FIFO_ERROR;
	
	
}



// Ensures a SendArm[7/9]Command (FIFO message) command to be forcefully executed at target ARM Core, while the host ARM Core awaits. 
#ifdef ARM9
__attribute__((section(".itcm")))
#endif
void FIFO_DRAINWRITE(){
	while (!(REG_IPC_FIFO_CR & IPC_FIFO_SEND_EMPTY)){}	
}

//FIFO HANDLER END

#ifdef ARM9
void apusetup(){
	MyIPC->IPC_ADDR = (u32*)ipc_region;
	MyIPC->apu_ready = true;
}
#endif

/*
//coto: in case you're wondering, these opcodes allow to reach specific region memory that is not available from the other core.

// u32 address, u8 read_mode (0 : u32 / 1 : u16 / 2 : u8)
#ifdef ARM9
__attribute__((section(".itcm")))
#endif
inline u32 read_ext_cpu(u32 address,u8 read_mode){
    #ifdef ARM7
        MyIPC->status |= ARM9_BUSYFLAGRD;
        SendArm9Command(0xc2720000, address, read_mode,0x00000000);
        while(MyIPC->status & ARM9_BUSYFLAGRD){}
    #endif
        
    #ifdef ARM9
        MyIPC->status |= ARM7_BUSYFLAGRD;
        SendArm7Command(0xc2720000, address, read_mode,0x00000000);
        while(MyIPC->status & ARM7_BUSYFLAGRD){}
    #endif
    
    return (u32)MyIPC->buf_queue[0];
}

//Direct writes: Write ARMx<->ARMx opcodes:
// u32 address, u8 write_mode (0 : u32 / 1 : u16 / 2 : u8)
#ifdef ARM9
__attribute__((section(".itcm")))
#endif
inline void write_ext_cpu(u32 address,u32 value,u8 write_mode){

    #ifdef ARM7
        MyIPC->status |= ARM9_BUSYFLAGWR;
        SendArm9Command(0xc2720001, address, write_mode, value);
        while(MyIPC->status& ARM9_BUSYFLAGWR){}
    #endif
        
    #ifdef ARM9
        MyIPC->status |= ARM7_BUSYFLAGWR;
        SendArm7Command(0xc2720001, address, write_mode, value);
        while(MyIPC->status& ARM7_BUSYFLAGWR){}
    #endif
    
}

//NDS hardware IPC
void sendbyte_ipc(uint8 word){
	//checkreg writereg (add,val) static int REG_IPC_add=0x04000180,REG_IE_add=0x04000210,REG_IF_add=0x04000214;
	*((u32*)0x04000180)=((*(u32*)0x04000180)&0xfffff0ff) | (word<<8);
}

u8 recvbyte_ipc(){
	return ((*(u32*)0x04000180)&0xf);
}

*/



//coto: humble ipc clock opcodes
u8 gba_get_yearbytertc(){
	return (u8)(u32)MyIPC->clockdata[0];
}

u8 gba_get_monthrtc(){
	return (u8)(u32)MyIPC->clockdata[1];
}

u8 gba_get_dayrtc(){
	return (u8)(u32)MyIPC->clockdata[2];
}

u8 gba_get_dayofweekrtc(){
	return (u8)(u32)MyIPC->clockdata[3];
}


u8 gba_get_hourrtc(){
	return (u8)(u32)MyIPC->clockdata[4];
}

u8 gba_get_minrtc(){
	return (u8)(u32)MyIPC->clockdata[5];
}

u8 gba_get_secrtc(){
	return (u8)(u32)MyIPC->clockdata[6];
}