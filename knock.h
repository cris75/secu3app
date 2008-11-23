
#ifndef _KNOCK_H_
#define _KNOCK_H_

//��������� ������� ��������� ������ � ������������ � �������� ��������� 
//HIP9011. ����������� ���������� ��������� ������ � ������ HOLD!

//��� ��� ������� ����� ���������� � �������� ������� � ��� ����� �����
//�������� ���������. ������ ���������� ���������������� �������� ������ �����������
//������ ����� ������ knock_start_settings_latching()
//��������� ����������� ������� ���������� �������
void knock_set_band_pass(unsigned char freq);
//��������� �������� ���������������� ���������
void knock_set_gain(unsigned char gain);
//��������� ������� ��������������
void knock_set_integration_time(unsigned char inttime);

//��������� ������� �������� �������� � ���������� ���������. ������ ���������� 
//��� ������������ ����� �������� ���������, ��� ������� ���������� ���������
//��������� � ��������� HOLD.
void knock_start_settings_latching(void);
//���������� �� 0 ���� � ������� ������ �������� �������� �� �����������
unsigned char knock_is_latching_idle(void);

//���������� ������ ��������� ���������
void knock_module_initialize(void);

//������������ �� ���� INT/HOLD HIP-a, ������������ ����� �������  
//���� ����� ��������������, ���� ����� �����������.
#define KNOCK_INTMODE_INT  1
#define KNOCK_INTMODE_HOLD 0
void knock_set_integration_mode(char mode); 

#endif //_KNOCK_H_
