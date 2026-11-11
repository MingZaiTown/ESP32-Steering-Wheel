#ifndef __ENCODER_H
#define __ENCODER_H

#define ENC_GPIO_PHASE_A GPIO_NUM_4
#define ENC_GPIO_PHASE_B GPIO_NUM_5
#define ENC_CNT_PER_CIRCLE 1320
#define ENC_CNT_LIMIT (2.5 * ENC_CNT_PER_CIRCLE)

void enc_init(void);
int enc_get_count(void);
int enc_get_count_test(void);
float enc_get_angle(void);

#endif
