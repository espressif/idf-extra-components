#pragma once

static inline void esp_foc_modulate_dq_voltage (float vbus_bias,
                                            float theta,
                                            float v_d,
                                            float v_q,
                                            float *v_u,
                                            float *v_v,
                                            float *v_w)
{
    float dq_frame[2] = {v_d, v_q};
    float ab_frame[2];

    esp_foc_inverse_park_transform(theta, dq_frame, &ab_frame[0], &ab_frame[1]);
    esp_foc_inverse_clarke_transform(ab_frame, v_u,v_v,v_w);

    *v_u += vbus_bias;
    *v_v += vbus_bias;
    *v_w += vbus_bias;   
}

static inline void esp_foc_get_dq_currents(float theta,
                                        float i_u, 
                                        float i_v, 
                                        float i_w, 
                                        float *i_q, 
                                        float *i_d) {
    
    float phase_current_frame[3] = {i_u, i_v, i_w};
    float ab_frame[2];
    esp_foc_clarke_transform(phase_current_frame,&ab_frame[0], &ab_frame[1]);
    esp_foc_park_transform(theta, ab_frame, i_d, i_q);
}
