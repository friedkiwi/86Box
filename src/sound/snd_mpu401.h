/*
 * 86Box	A hypervisor and IBM PC system emulator that specializes in
 *		running old operating systems and software designed for IBM
 *		PC systems and compatibles from 1981 through fairly recent
 *		system designs based on the PCI bus.
 *
 *		This file is part of the 86Box distribution.
 *
 *		Roland MPU-401 emulation.
 *
 * Version:	@(#)sound_mpu401.h	1.0.3	2018/09/11
 *
 * Author:	Sarah Walker, <http://pcem-emulator.co.uk/>
 *		DOSBox Team,
 *		Miran Grca, <mgrca8@gmail.com>
 *		TheCollector1995, <mariogplayer@gmail.com>
 *		Copyright 2008-2018 Sarah Walker.
 *		Copyright 2008-2018 DOSBox Team.
 *		Copyright 2016-2018 Miran Grca.
 *		Copyright 2016-2018 TheCollector1995.
 */

#define MPU401_VERSION	0x15
#define MPU401_REVISION	0x01
#define MPU401_QUEUE 32
#define MPU401_TIMECONSTANT (60000000/1000.0f)
#define MPU401_RESETBUSY 27.0f

typedef enum MpuMode
{
    M_UART,
    M_INTELLIGENT
} MpuMode;

#define M_MCA 0x10

typedef enum MpuDataType
{
    T_OVERFLOW,
    T_MARK,
    T_MIDI_SYS,
    T_MIDI_NORM,
    T_COMMAND
} MpuDataType;

/* Messages sent to MPU-401 from host */
#define MSG_EOX	                        0xf7
#define MSG_OVERFLOW                    0xf8
#define MSG_MARK                        0xfc

/* Messages sent to host from MPU-401 */
#define MSG_MPU_OVERFLOW                0xf8
#define MSG_MPU_COMMAND_REQ             0xf9
#define MSG_MPU_END                     0xfc
#define MSG_MPU_CLOCK                   0xfd
#define MSG_MPU_ACK                     0xfe

typedef struct mpu_t
{
    int uart_mode, intelligent,
	irq,
	queue_pos, queue_used;
    uint8_t rx_data, is_mca,
	    status,
	    queue[MPU401_QUEUE], pos_regs[8];
    MpuMode mode;
    struct track 
    {
	int counter;
	uint8_t value[8], sys_val,
		vlength,length;
	MpuDataType type;
    } playbuf[8], condbuf;
    struct {
	int conductor, cond_req,
	    cond_set, block_ack,
	    playing, reset,
	    wsd, wsm, wsd_start,
	    run_irq, irq_pending,
	    send_now, eoi_scheduled,
	    data_onoff;
	uint8_t tmask, cmask,
		amask,
		channel, old_chan;
	uint16_t midi_mask, req_mask;
	uint32_t command_byte, cmd_pending;
    } state;
    struct {
	uint8_t timebase, old_timebase,
		tempo, old_tempo,
		tempo_rel, old_tempo_rel,
		tempo_grad,
		cth_rate, cth_counter;
	int clock_to_host,cth_active;
    } clock;
	
	pc_timer_t mpu401_event_callback, mpu401_eoi_callback, 
			mpu401_reset_callback;
} mpu_t;

extern int	mpu401_standalone_enable;
extern const device_t	mpu401_device;
extern const device_t	mpu401_mca_device;


extern uint8_t	MPU401_ReadData(mpu_t *mpu);
extern void	mpu401_init(mpu_t *mpu, uint16_t addr, int irq, int mode);
extern void	mpu401_device_add(void);
