#pragma once

typedef struct {
    float alpha;
    float beta;
    float y_n_prev;
} esp_foc_lp_filter_t;

static inline void esp_foc_low_pass_filter_init(esp_foc_lp_filter_t *filter,
                                                float alpha)
{
    if(alpha > 1.0f) {
        alpha = 1.0f;
    } else if (alpha < 0.0f) {
        alpha = 0.0f;
    } 

    filter->y_n_prev = 0.0f;
    filter->alpha = alpha;
    filter->beta = 1.0f - alpha;

}

static inline float esp_foc_low_pass_filter_update(esp_foc_lp_filter_t *filter,
                                                  float x_n)
{
    float y_n = filter->alpha * x_n + filter->beta * filter->y_n_prev;
    filter->y_n_prev = y_n;
    return y_n;
}