/* SECU-3  - An open source, free engine control unit
   Copyright (C) 2007 Alexey A. Shabelnikov. Ukraine, Kiev

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

   contacts:
              http://secu-3.org
              email: shabelnikov@secu-3.org
*/

/** \file ckps2ch.c
 * \author Alexey A. Shabelnikov
 * Implementation of crankshaft position sensor's processing (2ch igniter version).
 * (���������� ��������� ������� ��������� ���������, ������ ��� 2-� ���������� �����������).
 */

#ifdef CKPS_2CHIGN
#if defined(HALL_SYNC)
 #error "You can not use HALL_SYNC option together with CKPS_2CHIGN!"
#endif
#if defined(CKPS_NPLUS1)
 #error "You can not use CKPS_NPLUS1 option together with CKPS_2CHIGN!"
#endif

#include <stdlib.h>
#include "port/avrio.h"
#include "port/interrupt.h"
#include "port/intrinsic.h"
#include "port/port.h"
#include "bitmask.h"
#include "camsens.h"
#include "ckps.h"
#include "injector.h"   //inject_start_inj()
#include "ioconfig.h"
#include "magnitude.h"
#include "tables.h"     //fnptr_t

#include "knock.h"

//PHASED_IGNITION and DWELL_CONTROL can't be used in conjunction with 2 channel igniter (single input, driven by 2 edges)
#if defined(PHASED_IGNITION) || defined(DWELL_CONTROL)
 #error "You can not use phased ignition or dwell control in conjunction with 2ch igniter (1 input, driven by 2 edges)"
#endif

/**Maximum number of ignition channels */
#define IGN_CHANNELS_MAX      8

/** Barrier for detecting of missing teeth (������ ��� �������� �����������) 
 * e.g. for 60-2 crank wheel, p * 2.5
 *      for 36-1 crank wheel, p * 1.5
 */
#define CKPS_GAP_BARRIER(p) ( ((p) << (ckps.miss_cogs_num==2)) + ((p) >> 1) )

/** number of teeth that will be skipped at the start before synchronization
 * (���������� ������ ������� ����� ������������ ��� ������ ����� ��������������) */
#define CKPS_ON_START_SKIP_COGS      5

/** Access Input Capture Register */
#define GetICR() (ICR1)

/** Used to indicate that none from ignition channels are selected
 * (������������ ��� �������� ���� ��� �� ���� ����� ��������� �� ������) */
#define CKPS_CHANNEL_MODENA  255

//Define values for controlling of outputs
#define IGN_OUTPUTS_INIT_VAL 1        //!< value used for initialization
#define IGN_OUTPUTS_ON_VAL   1        //!< value used to turn on ignition channel
#define IGN_OUTPUTS_OFF_VAL  0        //!< value used to turn off ignition channel

/**�������� ����� � ���������� COMPA � ��������� ������ �� �����. ����� ����� � ����� �������.
 * ������������ ��� ����������� �������. */
#define COMPA_VECT_DELAY 2

// Flags (see flags variable)
#define F_ERROR     0                 //!< CKP error flag, set in the CKP's interrupt, reset after processing (������� ������ ����, ��������������� � ���������� �� ����, ������������ ����� ���������) 
#define F_VHTPER    1                 //!< used to indicate that measured period is valid (actually measured)
#define F_ISSYNC    2                 //!< indicates that synchronization has been completed (missing teeth found)
#define F_STROKE    3                 //!< flag for synchronization with rotation (���� ������������� � ���������)
#define F_USEKNK    4                 //!< flag which indicates using of knock channel (������� ������������� ������ ���������)
#define F_NTSCHA    5                 //!< indicates that it is necessary to set channel
#define F_IGNIEN    7                 //!< Ignition enabled/disabled

//Additional flags (see flags2 variable)
#define F_CALTIM     2                //!< Indicates that time calculation is started before the spark
#define F_SPSIGN     3                //!< Sign of the measured stroke period (time between TDCs)

/** State variables */
typedef struct
{
 uint16_t icr_prev;                   //!< previous value if Input Capture Register (���������� �������� �������� �������)
 volatile uint16_t period_curr;       //!< last measured inter-tooth period (�������� ���������� ��������� ������)
 uint16_t period_prev;                //!< previous value of inter-tooth period (���������� �������� ���������� �������)
 volatile uint16_t cog;               //!< counts teeth starting from missing teeth (2 revolutions), begins from 1 (������� ����� ����� ������, �������� ������� � 1)
 volatile uint8_t cog360;             //!< counts teeth starting from missing teeth (1 revolution).
 uint16_t measure_start_value;        //!< remembers the value of the capture register to measure the half-turn (���������� �������� �������� ������� ��� ��������� ������� �����������)
 uint16_t current_angle;              //!< counts out given advance angle during the passage of each tooth (����������� �������� ��� ��� ����������� ������� ����)
 volatile uint16_t stroke_period;     //!< stores the last measurement of the passage of teeth n (������ ��������� ��������� ������� ����������� n ������)
 int16_t  advance_angle;              //!< required adv.angle * ANGLE_MULTIPLIER (��������� ��� * ANGLE_MULTIPLIER)
 volatile int16_t advance_angle_buffered;//!< buffered value of advance angle (to ensure correct latching)
 uint8_t  starting_mode;              //!< state of state machine processing of teeth at the startup (��������� ��������� �������� ��������� ������ �� �����)
 uint8_t  channel_mode;               //!< determines which channel of the ignition to run at the moment (���������� ����� ����� ��������� ����� ��������� � ������ ������)
 volatile uint8_t cogs_btdc;          //!< number of teeth from missing teeth to TDC of the first cylinder (���-�� ������ �� ����������� �� �.�.� ������� ��������)
 int8_t   knock_wnd_begin_abs;        //!< begin of the phase selection window of detonation in the teeth of wheel, relatively to TDC (������ ���� ������� �������� ��������� � ������ ����� ������������ �.�.�)
 int8_t   knock_wnd_end_abs;          //!< end of the phase selection window of detonation in the teeth of wheel, relatively to TDC (����� ���� ������� �������� ��������� � ������ ����� ������������ �.�.�)
 volatile uint8_t chan_number;        //!< number of ignition channels (���-�� ������� ���������)
 uint32_t frq_calc_dividend;          //!< divident for calculating RPM (������� ��� ������� ������� ��������)
#ifdef HALL_OUTPUT
 int8_t   hop_offset;                 //!< Hall output: start of pulse in tooth of wheel relatively to TDC
 uint8_t  hop_duration;               //!< Hall output: duration of pulse in tooth of wheel
#endif
#ifdef FUEL_INJECT
 int16_t  inj_phase;                  //!< Injection timing: start of pulse in teeth of wheel relatively to TDC
#endif

 volatile uint8_t wheel_cogs_num;     //!< Number of teeth, including absent (���������� ������, ������� �������������)
 volatile uint8_t wheel_cogs_nump1;   //!< wheel_cogs_num + 1
 volatile uint8_t wheel_cogs_numm1;   //!< wheel_cogs_num - 1
 volatile uint16_t wheel_cogs_num2;   //!< Number of teeth which corresponds to 720� (2 revolutions)
 volatile uint16_t wheel_cogs_num2p1; //!< wheel_cogs_num2 + 1
 volatile uint8_t miss_cogs_num;      //!< Count of crank wheel's missing teeth (���������� ������������� ������)
 volatile uint8_t wheel_last_cog;     //!< Number of last(present) tooth, numeration begins from 1! (����� ����������(�������������) ����, ��������� ���������� � 1!)
 /**Number of teeth before TDC which determines moment of advance angle latching, start of measurements from sensors,
  * latching of settings into HIP9011 (���-�� ������ �� �.�.� ������������ ������ �������� ���, ����� ��������� ��������,
  * �������� �������� � HIP)
  */
 volatile uint8_t  wheel_latch_btdc;
 volatile uint16_t degrees_per_cog;   //!< Number of degrees which corresponds to the 1 tooth (���������� �������� ������������ �� ���� ��� �����)
 volatile uint16_t cogs_per_chan;     //!< Number of teeth per 1 ignition channel (it is fractional number * 256)
 volatile int16_t start_angle;        //!< Precalculated value of the advance angle at 66� (at least) BTDC
#ifdef STROBOSCOPE
 uint8_t strobe;                      //!< Flag indicates that strobe pulse must be output on pending ignition stroke
#endif

 volatile uint8_t t1oc;               //!< Timer 1 overflow counter
 volatile uint8_t t1oc_s;             //!< Contains value of t1oc synchronized with stroke_period value
}ckpsstate_t;
 
/**Precalculated data (reference points) and state data for a single channel plug
 * ��������������� ������(������� �����) � ������ ��������� ��� ���������� ������ ���������
 */
typedef struct
{
 /**Address of first callback which will be used for settiong of I/O */
 volatile fnptr_t io_callback1;
 /**Address of second callback which will be used for settiong of I/O */
 volatile fnptr_t io_callback2;

#ifdef HALL_OUTPUT
 volatile uint16_t hop_begin_cog;      //!< Hall output: tooth number that corresponds to the beginning of pulse
 volatile uint16_t hop_end_cog;        //!< Hall output: tooth number that corresponds to the end of pulse
#endif

#ifdef FUEL_INJECT
 volatile uint16_t inj_begin_cog;      //!< Injection timing: tooth number that corresponds to the beginning of pulse
#endif

 /** Determines number of tooth (relatively to TDC) at which "latching" of data is performed (���������� ����� ���� (������������ �.�.�.) �� ������� ���������� "������������" ������) */
 volatile uint16_t cogs_latch;
 /** Determines number of tooth at which measurement of rotation period is performed (���������� ����� ���� �� ������� ������������ ��������� ������� �������� ��������� (����� ���. �������)) */
 volatile uint16_t cogs_btdc;
 /** Determines number of tooth at which phase selection window for knock detection is opened (���������� ����� ���� �� ������� ����������� ���� ������� �������� ������� �� (������ ��������������)) */
 volatile uint16_t knock_wnd_begin;
 /** Determines number of tooth at which phase selection window for knock detection is closed (���������� ����� ���� �� ������� ����������� ���� ������� �������� ������� �� (����� ��������������)) */
 volatile uint16_t knock_wnd_end;

 uint8_t output_state1;                //!< This variable specifies state of channel's output to be set, I/O1
 uint8_t output_state2;                //!< This variable specifies state of channel's output to be set, I/O2
}chanstate_t;

ckpsstate_t ckps;                         //!< instance of state variables
chanstate_t chanstate[IGN_CHANNELS_MAX];  //!< instance of array of channel's state variables

/** Arrange flags in the free I/O register (��������� � ��������� �������� �����/������) 
 *  note: may be not effective on other MCUs or even case bugs! Be aware.
 */
#define flags  GPIOR0                  //ATmega644 has one general purpose I/O register
#define flags2 TWBR

/** Supplement timer/counter 0 up to 16 bits, use R15 (��� ���������� �������/�������� 0 �� 16 ��������, ���������� R15) */
#ifdef __ICCAVR__
 __no_init __regvar uint8_t TCNT0_H@15;
#else //GCC
 uint8_t TCNT0_H __attribute__((section (".noinit")));
#endif

/**Table srtores dividends for calculating of RPM */
#define FRQ_CALC_DIVIDEND(channum) PGM_GET_DWORD(&frq_calc_dividend[channum])
PGM_DECLARE(uint32_t frq_calc_dividend[1+IGN_CHANNELS_MAX]) =
 //     1          2          3          4         5         6         7         8
 {0, 37500000L, 18750000L, 12500000L, 9375000L, 7500000L, 6250000L, 5357143L, 4687500L};

void ckps_init_state_variables(void)
{
 _BEGIN_ATOMIC_BLOCK();
 ckps.cog = ckps.cog360 = 0;
 ckps.stroke_period = 0xFFFF;
 ckps.advance_angle = ckps.advance_angle_buffered = 0;
 ckps.starting_mode = 0;
 ckps.channel_mode = CKPS_CHANNEL_MODENA;
 CLEARBIT(flags, F_NTSCHA);
 CLEARBIT(flags, F_STROKE);
 CLEARBIT(flags, F_ISSYNC);
 SETBIT(flags, F_IGNIEN);
 CLEARBIT(flags2, F_CALTIM);
 CLEARBIT(flags2, F_SPSIGN);

 TIMSK1|=_BV(TOIE1);                   //enable Timer 1 overflow interrupt. Used for correcting calculation of very low RPM

#ifdef STROBOSCOPE
 ckps.strobe = 0;
#endif

  ckps.t1oc = 0;                       //reset overflow counter
  ckps.t1oc_s = 255;                   //RPM is very low
 _END_ATOMIC_BLOCK();
}

void ckps_init_state(void)
{
 _BEGIN_ATOMIC_BLOCK();
 ckps_init_state_variables();
 CLEARBIT(flags, F_ERROR);

 //Compare channels do not connected to lines of ports (normal port mode)
 //(������ Compare �� ���������� � ������ ������ (���������� ����� ������))
 TCCR1A = 0;

 //(Noise reduction(���������� ����), rising edge of capture(�������� ����� �������), clock = 250kHz)
 TCCR1B = _BV(ICNC1)|_BV(ICES1)|_BV(CS11)|_BV(CS10);
 TCCR0B = _BV(CS01)|_BV(CS00); //clock = 312.5 kHz

 //enable input capture and Compare A interrupts of timer 1
 TIMSK1|= _BV(ICIE1);

 _END_ATOMIC_BLOCK();
}

void ckps_set_advance_angle(int16_t angle)
{
 _BEGIN_ATOMIC_BLOCK();
 ckps.advance_angle_buffered = angle;
 _END_ATOMIC_BLOCK();
}

void ckps_init_ports(void)
{
 IOCFG_INIT(IOP_CKPS, 1); // pullup for ICP1

 //after ignition is on, igniters must not be in the accumulation mode,
 //therefore set low level on their inputs
 //(����� ��������� ��������� ����������� �� ������ ���� � ������ ����������,
 //������� ������������� �� �� ������ ������ �������)
 IOCFG_INIT(IOP_IGN_OUT1, IGN_OUTPUTS_INIT_VAL);        //init 1-st (can be remapped)
 IOCFG_INIT(IOP_IGN_OUT2, IGN_OUTPUTS_INIT_VAL);        //init 2-nd (can be remapped)
 IOCFG_INIT(IOP_IGN_OUT3, IGN_OUTPUTS_INIT_VAL);        //init 3-rd (can be remapped)
 IOCFG_INIT(IOP_IGN_OUT4, IGN_OUTPUTS_INIT_VAL);        //init 4-th (can be remapped)
 IOCFG_INIT(IOP_ADD_IO1, IGN_OUTPUTS_INIT_VAL);         //init 5-th (can be remapped)
 IOCFG_INIT(IOP_ADD_IO2, IGN_OUTPUTS_INIT_VAL);         //init 6-th (can be remapped)
 IOCFG_INIT(IOP_IGN_OUT7, IGN_OUTPUTS_INIT_VAL);        //init 7-th (for maniacs)
 IOCFG_INIT(IOP_IGN_OUT8, IGN_OUTPUTS_INIT_VAL);        //init 8-th (for maniacs)

 //init I/O for Hall output if it is enabled
#ifdef HALL_OUTPUT
 IOCFG_INIT(IOP_HALL_OUT, 1);
#endif

 //init I/O for stroboscope
#ifdef STROBOSCOPE
 IOCFG_INIT(IOP_STROBE, 0);
#endif
}

//Instantaneous frequency calculation of crankshaft rotation from the measured period between the engine strokes
//(for example for 4-cylinder, 4-stroke it is 180�)
//Period measured in the discretes of timer (one discrete = 4us), one minute = 60 seconds, one second has 1,000,000 us.
//������������ ���������� ������� �������� ��������� �� ����������� ������� ����� ������� ���������
//(�������� ��� 4-������������, 4-� �������� ��� 180 ��������)
//������ � ��������� ������� (���� �������� = 4���), � ����� ������ 60 ���, � ����� ������� 1000000 ���.
uint16_t ckps_calculate_instant_freq(void)
{
 uint16_t period; uint8_t ovfcnt, sign;
 //ensure atomic acces to variable (������������ ��������� ������ � ����������)
 _DISABLE_INTERRUPT();
 period = ckps.stroke_period;        //stroke period
 ovfcnt = ckps.t1oc_s;               //number of timer overflows
 sign = CHECKBIT(flags2, F_SPSIGN);  //sign of stroke period
 _ENABLE_INTERRUPT();

 //We know period and number of timer overflows, so we can calculate correct value of RPM even if RPM is very low
 if (sign && ovfcnt > 0)
  return ckps.frq_calc_dividend / ((((int32_t)ovfcnt) * 65536) - (65536-period));
 else
  return ckps.frq_calc_dividend / ((((int32_t)ovfcnt) * 65536) + period);
}

void ckps_set_edge_type(uint8_t edge_type)
{
 _BEGIN_ATOMIC_BLOCK();
 if (edge_type)
  TCCR1B|= _BV(ICES1);
 else
  TCCR1B&=~_BV(ICES1);
 _END_ATOMIC_BLOCK();
}

/**
 * Ensures that tooth number will be in the allowed range.
 * Tooth number should not be greater than cogs number * 2 or less than zero
 */
static uint16_t _normalize_tn(int16_t i_tn)
{
 if (i_tn > (int16_t)ckps.wheel_cogs_num2)
  return i_tn - (int16_t)ckps.wheel_cogs_num2;
 if (i_tn <= 0)
  return i_tn + ckps.wheel_cogs_num2;
 return i_tn;
}

void ckps_set_cogs_btdc(uint8_t cogs_btdc)
{
 uint8_t _t, i;
 _t=_SAVE_INTERRUPT();
 _DISABLE_INTERRUPT();
 for(i = 0; i < ckps.chan_number; ++i)
 {
  uint16_t tdc = (((uint16_t)cogs_btdc) + ((i * ckps.cogs_per_chan) >> 8));
  chanstate[i].cogs_btdc = _normalize_tn(tdc);
  chanstate[i].cogs_latch = _normalize_tn(tdc - ckps.wheel_latch_btdc);
  chanstate[i].knock_wnd_begin = _normalize_tn(tdc + ckps.knock_wnd_begin_abs);
  chanstate[i].knock_wnd_end = _normalize_tn(tdc + ckps.knock_wnd_end_abs);
#ifdef HALL_OUTPUT
  //update Hall output pulse parameters because they depend on ckps.cogs_btdc parameter
  chanstate[i].hop_begin_cog = _normalize_tn(tdc - ckps.hop_offset);
  chanstate[i].hop_end_cog = _normalize_tn(chanstate[i].hop_begin_cog + ckps.hop_duration);
#endif
#ifdef FUEL_INJECT
  chanstate[i].inj_begin_cog = _normalize_tn(tdc - ckps.inj_phase);
#endif
 }
 ckps.cogs_btdc = cogs_btdc;
 _RESTORE_INTERRUPT(_t);
}

void ckps_set_ignition_cogs(uint8_t cogs)
{
 //not supported by this implementation
}

uint8_t ckps_is_error(void)
{
 return CHECKBIT(flags, F_ERROR) > 0;
}

void ckps_reset_error(void)
{
 CLEARBIT(flags, F_ERROR);
}

void ckps_use_knock_channel(uint8_t use_knock_channel)
{
 WRITEBIT(flags, F_USEKNK, use_knock_channel);
}

uint8_t ckps_is_stroke_event_r()
{
 uint8_t result;
 _BEGIN_ATOMIC_BLOCK();
 result = CHECKBIT(flags, F_STROKE) > 0;
 CLEARBIT(flags, F_STROKE);
 _END_ATOMIC_BLOCK();
 return result;
}

uint8_t ckps_is_cog_changed(void)
{
 static uint8_t prev_cog = 0;
 uint8_t value = ckps.cog;
 if (prev_cog != value)
 {
  prev_cog = value;
  return 1;
 }
 return 0;
}

/** Get value of I/O callback by index. This function is necessary for supporting of 7,8 ign. channels
 * \param index Index of callback */
INLINE
static fnptr_t get_callback(uint8_t index)
{
 return (index < IOP_ECF) ? IOCFG_CB(index) : IOCFG_CB(index + IOP_IGN78_OFF);
}

/**Tune channels' I/O for semi-sequential ignition mode (wasted spark) */
static void set_channels_ss(void)
{
 uint8_t _t, i;
 if (2 == ckps.chan_number || 4 == ckps.chan_number)
 { //4 cylinders
  for(i = 0; i < ckps.chan_number; ++i)
  {
   _t=_SAVE_INTERRUPT();
   _DISABLE_INTERRUPT();
   chanstate[i].io_callback1 = chanstate[i].io_callback2 = IOCFG_CB(0);
   chanstate[i].output_state1 = chanstate[i].output_state2 = (i & 1) ? IGN_OUTPUTS_OFF_VAL : IGN_OUTPUTS_ON_VAL;
   _RESTORE_INTERRUPT(_t);
  }
 }
 else if (6 == ckps.chan_number)
 {
   _t=_SAVE_INTERRUPT();
   _DISABLE_INTERRUPT();
    chanstate[0].io_callback1 = chanstate[3].io_callback1 = IOCFG_CB(0);
    chanstate[0].io_callback2 = chanstate[3].io_callback2 = IOCFG_CB(1);
    chanstate[0].output_state1 = chanstate[3].output_state1 = IGN_OUTPUTS_ON_VAL;
    chanstate[0].output_state2 = chanstate[3].output_state2 = IGN_OUTPUTS_OFF_VAL;
    chanstate[1].io_callback1 = chanstate[4].io_callback1 = IOCFG_CB(0);
    chanstate[1].io_callback2 = chanstate[4].io_callback2 = IOCFG_CB(1);
    chanstate[1].output_state1 = chanstate[4].output_state1 = IGN_OUTPUTS_OFF_VAL;
    chanstate[1].output_state2 = chanstate[4].output_state2 = IGN_OUTPUTS_OFF_VAL;
    chanstate[2].io_callback1 = chanstate[5].io_callback1 = IOCFG_CB(0);
    chanstate[2].io_callback2 = chanstate[5].io_callback2 = IOCFG_CB(1);
    chanstate[2].output_state1 = chanstate[5].output_state1 = IGN_OUTPUTS_OFF_VAL;
    chanstate[2].output_state2 = chanstate[5].output_state2 = IGN_OUTPUTS_ON_VAL;
   _RESTORE_INTERRUPT(_t);

 }
 else if (8 == ckps.chan_number)
 { //8 cylinders
  for(i = 0; i < ckps.chan_number; i+=2)
  {
    uint8_t state = (i & 2) ? IGN_OUTPUTS_OFF_VAL : IGN_OUTPUTS_ON_VAL;
   _t=_SAVE_INTERRUPT();
   _DISABLE_INTERRUPT();
    chanstate[i].io_callback1 = chanstate[i].io_callback2 = IOCFG_CB(0);
    chanstate[i].output_state1 = chanstate[i].output_state2 = state;
    chanstate[i + 1].io_callback1 = chanstate[i + 1].io_callback2 = IOCFG_CB(1);
    chanstate[i + 1].output_state1 = chanstate[i + 1].output_state2 = state;
   _RESTORE_INTERRUPT(_t);
  }
 }
}

void ckps_set_cyl_number(uint8_t i_cyl_number)
{
 uint8_t i = ckps.chan_number;
 _BEGIN_ATOMIC_BLOCK();
 ckps.chan_number = i_cyl_number;
 _END_ATOMIC_BLOCK();

 ckps.frq_calc_dividend = FRQ_CALC_DIVIDEND(i_cyl_number);

 //We have to retune I/O configuration after changing of cylinder number
 set_channels_ss();  // Tune for semi-sequential mode

 //unused channels must be turned off
 if (i > i_cyl_number)
  for(i = i_cyl_number; i < IGN_CHANNELS_MAX; ++i)
   ((iocfg_pfn_set)get_callback(i))(IGN_OUTPUTS_ON_VAL);

 //TODO: calculations previosly made by ckps_set_cogs_btdc()|ckps_set_knock_window()|ckps_set_hall_pulse() becomes invalid!
 //So, ckps_set_cogs_btdc() must be called again. Do it here or in place where this function called.
}

void ckps_set_knock_window(int16_t begin, int16_t end)
{
 uint8_t _t, i;
 //translate from degrees to teeth (��������� �� �������� � �����)
 ckps.knock_wnd_begin_abs = begin / ckps.degrees_per_cog;
 ckps.knock_wnd_end_abs = end / ckps.degrees_per_cog;

 _t=_SAVE_INTERRUPT();
 _DISABLE_INTERRUPT();
 for(i = 0; i < ckps.chan_number; ++i)
 {
  uint16_t tdc = (((uint16_t)ckps.cogs_btdc) + ((i * ckps.cogs_per_chan) >> 8));
  chanstate[i].knock_wnd_begin = _normalize_tn(tdc + ckps.knock_wnd_begin_abs);
  chanstate[i].knock_wnd_end = _normalize_tn(tdc + ckps.knock_wnd_end_abs);
 }
 _RESTORE_INTERRUPT(_t);
}

void ckps_enable_ignition(uint8_t i_cutoff)
{
 WRITEBIT(flags, F_IGNIEN, i_cutoff);
}

void ckps_set_merge_outs(uint8_t i_merge)
{
 //not supported by this implementation
}

#ifdef HALL_OUTPUT
void ckps_set_hall_pulse(int8_t i_offset, uint8_t i_duration)
{
 uint8_t _t, i;
 //save values because we will access them from other function
 ckps.hop_offset = i_offset;
 ckps.hop_duration = i_duration;

 _t=_SAVE_INTERRUPT();
 _DISABLE_INTERRUPT();
 for(i = 0; i < ckps.chan_number; ++i)
 {
  uint16_t tdc = (((uint16_t)ckps.cogs_btdc) + ((i * ckps.cogs_per_chan) >> 8));
  chanstate[i].hop_begin_cog = _normalize_tn(tdc - ckps.hop_offset);
  chanstate[i].hop_end_cog = _normalize_tn(chanstate[i].hop_begin_cog + ckps.hop_duration);
 }
 _RESTORE_INTERRUPT(_t);
}
#endif

void ckps_set_cogs_num(uint8_t norm_num, uint8_t miss_num)
{
 div_t dr; uint8_t _t;
#ifdef PHASE_SENSOR
 uint16_t err_thrd = (norm_num * 2) + (norm_num >> 3); //+ 12.5%
#endif
 uint16_t cogs_per_chan, degrees_per_cog;

 //precalculate number of cogs per 1 ignition channel, it is fractional number multiplied by 256
 cogs_per_chan = (((uint32_t)(norm_num * 2)) << 8) / ckps.chan_number;

 //precalculate value of degrees per 1 cog, it is fractional number multiplied by ANGLE_MULTIPLIER
 degrees_per_cog = (((((uint32_t)360) << 8) / norm_num) * ANGLE_MULTIPLIER) >> 8;

 //precalculate value and round it always to the upper bound,
 //e.g. for 60-2 crank wheel result = 11 (66�), for 36-1 crank wheel result = 7 (70�)
 dr = div(ANGLE_MAGNITUDE(66), degrees_per_cog);

 _t=_SAVE_INTERRUPT();
 _DISABLE_INTERRUPT();
 //calculate number of last cog
 ckps.wheel_last_cog = norm_num - miss_num;
 //set number of teeth (normal and missing)
 ckps.wheel_cogs_num = norm_num;
 ckps.wheel_cogs_nump1 = norm_num + 1;
 ckps.wheel_cogs_numm1 = norm_num - 1;
 ckps.miss_cogs_num = miss_num;
 ckps.wheel_cogs_num2 = norm_num * 2;
 ckps.wheel_cogs_num2p1 = (norm_num * 2) + 1;
 //set other precalculated values
 ckps.wheel_latch_btdc = dr.quot + (dr.rem > 0);
 ckps.degrees_per_cog = degrees_per_cog;
 ckps.cogs_per_chan = cogs_per_chan;
 ckps.start_angle = ckps.degrees_per_cog * ckps.wheel_latch_btdc;
#ifdef PHASE_SENSOR
 cams_set_error_threshold(err_thrd);
#endif
 _RESTORE_INTERRUPT(_t);
}

#ifdef FUEL_INJECT
void ckps_set_inj_timing(int16_t phase)
{
 uint8_t _t, i;
 //TODO: We can do some optimization in the future - set timing only if it is not equal to current (already set one)

 //convert from 0..720 BTDC to -360...360
 if (phase > ANGLE_MAGNITUDE(360))
  phase = phase;
 else
  phase = phase - ANGLE_MAGNITUDE(720);

 //save values because we will access them from other function
 //Also, convert form crank degrees to teeth
 ckps.inj_phase = phase / ((int16_t)ckps.degrees_per_cog);

 for(i = 0; i < ckps.chan_number; ++i)
 {
  uint16_t tdc = (((uint16_t)ckps.cogs_btdc) + ((i * ckps.cogs_per_chan) >> 8));
  uint16_t timing = _normalize_tn(tdc - ckps.inj_phase); //current inj.timing
 _t=_SAVE_INTERRUPT();
 _DISABLE_INTERRUPT();
  chanstate[i].inj_begin_cog = timing;
 _RESTORE_INTERRUPT(_t);
 }
}
#endif

/**Forces ignition spark if corresponding interrupt is pending*/
#define force_pending_spark() \
 if ((TIFR1 & _BV(OCF1A)) && (CHECKBIT(flags2, F_CALTIM)) && CHECKBIT(flags, F_IGNIEN))\
 { \
  ((iocfg_pfn_set)chanstate[ckps.channel_mode].io_callback1)(chanstate[ckps.channel_mode].output_state1); \
  ((iocfg_pfn_set)chanstate[ckps.channel_mode].io_callback2)(chanstate[ckps.channel_mode].output_state2); \
 }

/**Interrupt handler for Compare/Match channel A of timer T1
 * ������ ���������� �� ���������� ������ � ������� �1
 */
ISR(TIMER1_COMPA_vect)
{
 TIMSK1&= ~_BV(OCIE1A);//disable interrupt (��������� ����������)

 //line of port in the low level, now set it into a high level - makes the igniter to stop 
 //the accumulation of energy and close the transistor (spark)
 //(����� ����� � ������ ������, ������ ��������� � � ������� ������� - ���������� ���������� ����������
 //���������� ������� � ������� ���������� (�����)).
 if (CKPS_CHANNEL_MODENA == ckps.channel_mode)
  return; //none of channels selected (������� ����� �� ������)

#ifdef STROBOSCOPE
 if (1==ckps.strobe)
 {
  IOCFG_SET(IOP_STROBE, 1); //start pulse
  ckps.strobe = 2;          //and set flag to next state
  OCR1A = TCNT1 + 31;       //We will generate 100uS pulse
  TIMSK1|=_BV(OCIE1A);      //pulse will be ended in the next interrupt
 }
 else if (2==ckps.strobe)
 {
  IOCFG_SET(IOP_STROBE, 0); //end pulse
  ckps.strobe = 0;          //and reset flag
  return;
 }
#endif

 if (CHECKBIT(flags, F_IGNIEN)) //ignition disabled
 {
  ((iocfg_pfn_set)chanstate[ckps.channel_mode].io_callback1)(chanstate[ckps.channel_mode].output_state1);
  ((iocfg_pfn_set)chanstate[ckps.channel_mode].io_callback2)(chanstate[ckps.channel_mode].output_state2);
 }

 CLEARBIT(flags2, F_CALTIM); //we already output the spark, so calculation of time is finished
}

/**Initialize timer 0 using specified value and start it, clock = 312.5kHz
 * It is assumed that this function called when all interrupts are disabled
 * \param value Value to set timer for, 1 tick = 3.2uS
 */
INLINE
void set_timer0(uint16_t value)
{
  OCR0A = TCNT0 + _AB(value, 0);
  TCNT0_H = _AB(value, 1);
  SETBIT(TIMSK0, OCIE0A);
  SETBIT(TIFR0, OCF0A);
}

/**Helpful function, used at the startup of engine
 * (��������������� �������, ������������ �� ����� �����)
 * \return 1 when synchronization is finished, othrewise 0 (1 ����� ������������� ��������, ����� 0)
 */
static uint8_t sync_at_startup(void)
{
 switch(ckps.starting_mode)
 {
  case 0: //skip certain number of teeth (������� ������������� ���-�� ������)
   CLEARBIT(flags, F_VHTPER);
   if (ckps.cog >= CKPS_ON_START_SKIP_COGS)
    ckps.starting_mode = 1;
   break;

  case 1: //find out missing teeth (����� �����������)

   //if missing teeth = 0, then reference will be identified by additional VR sensor (REF_S input)
   if ((0==ckps.miss_cogs_num) ? cams_vr_is_event_r() : (ckps.period_curr > CKPS_GAP_BARRIER(ckps.period_prev)))
   {
    SETBIT(flags, F_ISSYNC);
    ckps.period_curr = ckps.period_prev;  //exclude value of missing teeth's period
    ckps.cog = ckps.cog360 = 1; //first tooth (1-� ���)
    return 1; //finish process of synchronization (����� �������� �������������)
   }
   break;
 }
 ckps.icr_prev = GetICR();
 ckps.period_prev = ckps.period_curr;
 ++ckps.cog;
 return 0; //continue process of synchronization (����������� �������� �������������)
}

/**This procedure called for all teeth (including recovered teeth)
 * ���������. ���������� ��� ���� ������ ����� (������������ � ����������������)
 */
static void process_ckps_cogs(void)
{
 uint8_t i;

 force_pending_spark();

 for(i = 0; i < ckps.chan_number; ++i)
 {
  if (CHECKBIT(flags, F_USEKNK))
  {
   //start listening a detonation (opening the window)
   //�������� ������� ��������� (�������� ����)
   if (ckps.cog == chanstate[i].knock_wnd_begin)
    knock_set_integration_mode(KNOCK_INTMODE_INT);

   //finish listening a detonation (closing the window) and start the process of measuring integrated value
   //����������� ������� ��������� (�������� ����) � ��������� ������� ��������� ������������ ��������
   if (ckps.cog == chanstate[i].knock_wnd_end)
   {
    knock_set_integration_mode(KNOCK_INTMODE_HOLD);
    adc_begin_measure_knock(_AB(ckps.stroke_period, 1) < 4);
   }
  }

  //for 66� before TDC (before working stroke) establish new advance angle to be actuated,
  //before this moment value was stored in a temporary buffer.
  //�� 66 �������� �� �.�.� ����� ������� ������ ������������� ����� ��� ��� ����������, ���
  //�� ����� �������� �� ��������� ������.
  if (ckps.cog == chanstate[i].cogs_latch)
  {
   ckps.channel_mode = i;                    //remember number of channel (���������� ����� ������)
   SETBIT(flags, F_NTSCHA);                  //establish an indication that it is need to count advance angle (������������� ������� ����, ��� ����� ����������� ���)
   //start counting of advance angle (�������� ������ ���� ����������)
   ckps.current_angle = ckps.start_angle; // those same 66� (�� ����� 66�)
   ckps.advance_angle = ckps.advance_angle_buffered; //advance angle with all the adjustments (say, 15�)(���������� �� ����� ��������������� (��������, 15�))
   knock_start_settings_latching();//start the process of downloading the settings into the HIP9011 (��������� ������� �������� �������� � HIP)
   adc_begin_measure(_AB(ckps.stroke_period, 1) < 4);//start the process of measuring analog input values (������ �������� ��������� �������� ���������� ������)
#ifdef STROBOSCOPE
   if (0==i)
    ckps.strobe = 1; //strobe!
#endif
  }

  force_pending_spark();

  //teeth of end/beginning of the measurement of rotation period - TDC Read and save the measured period,
  //then remember current value of count for the next measurement
  //(����� ����������/������ ��������� �������� ��������  - �.�.�. ���������� � ���������� ����������� �������,
  //����� ����������� �������� �������� �������� ��� ���������� ���������)
  if (ckps.cog==chanstate[i].cogs_btdc)
  {
   //save period value if it is correct
   if (CHECKBIT(flags, F_VHTPER))
   {
    ckps.stroke_period = (GetICR() - ckps.measure_start_value);
    WRITEBIT(flags2, F_SPSIGN, GetICR() < ckps.measure_start_value); //save sign
    ckps.t1oc_s = ckps.t1oc, ckps.t1oc = 0; //save value and reset counter
   }

   ckps.measure_start_value = GetICR();
   SETBIT(flags, F_VHTPER);
   SETBIT(flags, F_STROKE); //set the stroke-synchronozation event (������������� ������� �������� �������������)
  }

#ifdef HALL_OUTPUT
  if (ckps.cog == chanstate[i].hop_begin_cog)
   IOCFG_SET(IOP_HALL_OUT, 1);
  if (ckps.cog == chanstate[i].hop_end_cog)
   IOCFG_SET(IOP_HALL_OUT, 0);
#endif

#ifdef FUEL_INJECT
  if (ckps.cog == chanstate[i].inj_begin_cog)
   inject_start_inj(i);      //start fuel injection for current channel
#endif
 }

 force_pending_spark();

 //Preparing to start the ignition for the current channel (if the right moment became)
 //���������� � ������� ��������� ��� �������� ������ (���� �������� ������ ������)
 if (CHECKBIT(flags, F_NTSCHA) && ckps.channel_mode!= CKPS_CHANNEL_MODENA)
 {
  uint16_t diff = ckps.current_angle - ckps.advance_angle;
  if (diff <= (ckps.degrees_per_cog << 1))
  {
   //before starting the ignition it is left to count less than 2 teeth. It is necessary to prepare the compare module
   //(�� ������� ��������� �������� ��������� ������ 2-x ������. ���������� ����������� ������ ���������)
   //TODO: replace heavy division by multiplication with magic number. This will reduce up to 40uS !
   if (ckps.period_curr < 128)
    OCR1A = GetICR() + ((diff * (ckps.period_curr)) / ckps.degrees_per_cog) - COMPA_VECT_DELAY;
   else
    OCR1A = GetICR() + (((uint32_t)diff * (ckps.period_curr)) / ckps.degrees_per_cog) - COMPA_VECT_DELAY;
   TIFR1 = _BV(OCF1A);
   TIMSK1|= _BV(OCIE1A);      // enable Compare A interrupt (��������� ����������)
   CLEARBIT(flags, F_NTSCHA); // For avoiding to enter into setup mode (����� �� ����� � ����� ��������� ��� ���)
   SETBIT(flags2, F_CALTIM);  // Set indication that we begin to calculate the time
  }
 }

 //tooth passed - angle before TDC decriased (e.g 6� per tooth for 60-2).
 //(������ ��� - ���� �� �.�.�. ���������� (�������� 6� �� ��� ��� 60-2)).
 ckps.current_angle-= ckps.degrees_per_cog;
 ++ckps.cog;

#ifdef PHASE_SENSOR
 //search for level's toggle from camshaft sensor on each cog
 cams_detect_edge();
#endif

 force_pending_spark();
}

/**Input capture interrupt of timer 1 (called at passage of each tooth)
 * ���������� �� ������� ������� 1 (���������� ��� ����������� ���������� ����)
 */
ISR(TIMER1_CAPT_vect)
{
 force_pending_spark();

 ckps.period_curr = GetICR() - ckps.icr_prev;

 //At the start of engine, skipping a certain number of teeth for initializing
 //the memory of previous periods. Then look for missing teeth.
 //��� ������ ���������, ���������� ������������ ���-�� ������ ��� �������������
 //������ ���������� ��������. ����� ���� �����������.
 if (!CHECKBIT(flags, F_ISSYNC))
 {
  if (sync_at_startup())
   goto sync_enter;
  return;
 }

 //if missing teeth = 0, then reference will be identified by additional VR sensor (REF_S input),
 //Otherwise:
 //Each period, check for missing teeth, and if, after discovering of missing teeth
 //count of teeth being found incorrect, then set error flag.
 //(������ ������ ��������� �� �����������, � ���� ����� ����������� �����������
 //��������� ��� ���-�� ������ ������������, �� ������������� ������� ������).
 if ((0==ckps.miss_cogs_num) ? cams_vr_is_event_r() : (ckps.period_curr > CKPS_GAP_BARRIER(ckps.period_prev)))
 {
  if ((ckps.cog360 != ckps.wheel_cogs_nump1)) //also taking into account recovered teeth (��������� ����� ��������������� �����)
  {
   SETBIT(flags, F_ERROR); //ERROR
   ckps.cog = 1;
   //TODO: maybe we need to turn off full sequential mode
  }
  //Reset 360� tooth counter to the first tooth (1-� ���)
  ckps.cog360 = 1;
  //Also reset 720� tooth counter
  if (ckps.cog == ckps.wheel_cogs_num2p1)
   ckps.cog = 1;
  ckps.period_curr = ckps.period_prev;  //exclude value of missing teeth's period (for missing teeth only)
 }

sync_enter:
 //If the last tooth before missing teeth, we begin the countdown for
 //the restoration of missing teeth, as the initial data using the last
 //value of inter-teeth period.
 //(���� ��������� ��� ����� ������������, �� �������� ������ ������� ���
 //�������������� ������������� ������, � �������� �������� ������ ����������
 //��������� �������� ���������� �������).
 if (ckps.miss_cogs_num && ckps.cog360 == ckps.wheel_last_cog)
  set_timer0(ckps.period_curr);

 //call handler for normal teeth (�������� ���������� ��� ���������� ������)
 process_ckps_cogs();
 ++ckps.cog360;

 ckps.icr_prev = GetICR();
 ckps.period_prev = ckps.period_curr;

 force_pending_spark();
}

/**Purpose of this interrupt handler is to supplement timer up to 16 bits and call procedure
 * for processing teeth when set 16 bit timer expires
 * (������ ����� ����������� ��������� ������ �� 16-�� �������� � �������� ���������
 * ��������� ������ �� ��������� �������������� 16-�� ���������� �������). */
ISR(TIMER0_COMPA_vect)
{
 if (TCNT0_H!=0)  //Did high byte exhaust (������� ���� �� ��������) ?
 {
  TCNT0 = 0;
  --TCNT0_H;
 }
 else
 {//the countdown is over (������ ������� ����������)
  ICR1 = TCNT1;  //simulate input capture
  CLEARBIT(TIMSK0, OCIE0A); //disable this interrupt

  if (ckps.miss_cogs_num > 1)
  {
   //start timer to recover 60th tooth (��������� ������ ����� ������������ 60-� (���������) ���)
   if (ckps.cog360 == ckps.wheel_cogs_numm1)
    set_timer0(ckps.period_curr);
  }

  //Call handler for missing teeth (�������� ���������� ��� ������������� ������)
  process_ckps_cogs();
  ++ckps.cog360;
 }
}

/** Timer 1 overflow interrupt.
 * Used to count timer 1 overflows to obtain correct revolution period at very low RPMs (10...400 min-1)
 */
ISR(TIMER1_OVF_vect)
{
 ++ckps.t1oc;
}

#endif //CKPS_2CHIGN
