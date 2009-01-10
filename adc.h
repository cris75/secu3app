
#ifndef _ADC_H_
#define _ADC_H_

#define ADC_DISCRETE            0.0025       //���� �������� ��� � �������

#define TSENS_SLOPP             0.01        //������ ������ ������� ����������� �����/������
#define TSENS_ZERO_POINT        2.73        //���������� �� ������ ������� ����������� ��� 0 �������� �������

#define ADC_VREF_TYPE           0xC0

//������ ������������ ������� ���
#define ADCI_MAP                2
#define ADCI_UBAT               1         
#define ADCI_TEMP               0
#define ADCI_KNOCK              3
#define ADCI_STUB               4  //��������, ������������ ��� ADCI_KNOCK

#define MAP_PHYSICAL_MAGNITUDE_MULTIPLAYER  64
#define UBAT_PHYSICAL_MAGNITUDE_MULTIPLAYER (1.0/ADC_DISCRETE) //=400
#define TEMP_PHYSICAL_MAGNITUDE_MULTIPLAYER (TSENS_SLOPP / ADC_DISCRETE) //=4

//��� ������� ���������� ������� �������� �� ������� ����������
unsigned int adc_get_map_value(void);
unsigned int adc_get_ubat_value(void);
unsigned int adc_get_temp_value(void);
unsigned int adc_get_knock_value(void);

//��������� ��������� �������� � ��������, �� ������ ���� ����������  
//��������� ���������.
void adc_begin_measure(void);
//��������� ��������� �������� � ����������� ������ ���������. ��� ��� ����� ���������
//������� INT/HOLD � 0 ����� INTOUT �������� � ��������� ���������� ��������� ������ �����
//20��� (��������������), � ������ ��������� ����� ���� ���������� �����, �� ������ ������
//��������� ��������.
void adc_begin_measure_knock(void);

//���������� �� 0 ���� ��������� ������ (��� �� ������)
char adc_is_measure_ready(void); 

//������������� ��� � ��� ���������� ���������
void adc_init(void);

signed int adc_compensate(signed int adcvalue, signed int factor, signed long correction);

//��������� �������� ��� � ���������� �������� - ��������
//���������� �������� * MAP_PHYSICAL_MAGNITUDE_MULTIPLAYER
unsigned int map_adc_to_kpa(signed int adcvalue, unsigned int offset, unsigned int gradient);

//��������� �������� ��� � ���������� �������� - ����������
//���������� �������� * UBAT_PHYSICAL_MAGNITUDE_MULTIPLAYER
unsigned int ubat_adc_to_v(signed int adcvalue);

//��������� �������� ��� � ���������� �������� - �����������
//���������� �������� * TEMP_PHYSICAL_MAGNITUDE_MULTIPLAYER
signed int temp_adc_to_c(signed int adcvalue);

#endif //_ADC_H_
