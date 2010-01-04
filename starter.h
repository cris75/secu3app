
#ifndef _STARTER_H_
#define _STARTER_H_

struct ecudata;

//инициализация используемых портов
void starter_init_ports(void);

//управление стартером
void starter_control(struct ecudata* d);

//блокировка/разблокировка стартера
void starter_set_blocking_state(uint8_t i_state);

#endif //_STARTER_H_
