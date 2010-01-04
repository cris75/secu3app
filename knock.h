
#ifndef _KNOCK_H_
#define _KNOCK_H_

#include <stdint.h>

//��������� ������� ��������� ������ � ������������ � �������� ��������� 
//HIP9011. ����������� ���������� ��������� ������ � ������ HOLD!

//��� ��� ������� ����� ���������� � �������� ������� � ��� ����� �����
//�������� ���������. ������ ���������� ���������������� �������� ������ �����������
//������ ����� ������ knock_start_settings_latching()
//��������� ����������� ������� ���������� �������
void knock_set_band_pass(uint8_t freq);
//��������� �������� ���������������� ���������
void knock_set_gain(uint8_t gain);
//��������� ���������� ������� �����������
void knock_set_int_time_constant(uint8_t inttime);

//��������� ������� �������� �������� � ���������� ���������. ������ ���������� 
//��� ������������ ����� �������� ���������, ��� ������� ���������� ���������
//��������� � ��������� HOLD. ���� �� ������ ������ ���� ������� ������� ��������
//��� �� ��������, �� ������ ������� ����� ������� � ������� �����, �� ��� ���� 
//����� ���������� ������� ������.
void knock_start_settings_latching(void);
//���������� �� 0 ���� � ������� ������ �������� �������� �� �����������
uint8_t knock_is_latching_idle(void);

//���������� 1 ���� ���� ������ (���������� ���������� ��� ���� ���������� ��������� ������)
uint8_t knock_is_error(void);

//����� ������
void knock_reset_error(void);

//���������� ������ ��������� � ��� ������������.
//���������� 1 - ���� ������������ ������ �������, ����� 0. 
uint8_t knock_module_initialize(void);

//������������ �� ���� INT/HOLD HIP-a, ������������ ����� �������  
//���� ����� ��������������, ���� ����� �����������.
#define KNOCK_INTMODE_INT  1
#define KNOCK_INTMODE_HOLD 0
void knock_set_integration_mode(uint8_t mode); 

//������������� ������������ ������ �����/������
void knock_init_ports(void);

#endif //_KNOCK_H_
