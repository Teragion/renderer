#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../core/api.h"
#include "test_helper.h"

static const char *WINDOW_TITLE = "Viewer";
static const int WINDOW_WIDTH = 800;
static const int WINDOW_HEIGHT = 600;

static const vec3_t CAMERA_POSITION = {0, 0, 2};
static const vec3_t CAMERA_TARGET = {0, 0, 0};

static const float LIGHT_THETA = TO_RADIANS(45);
static const float LIGHT_PHI = TO_RADIANS(45);
static const float LIGHT_SPEED = PI;

typedef struct {
    motion_t motion;
    int orbiting, panning;
    vec2_t orbit_pos, pan_pos;
    float light_theta, light_phi;
} record_t;

static vec2_t calculate_delta(vec2_t old_pos, vec2_t new_pos) {
    vec2_t delta = vec2_sub(new_pos, old_pos);
    return vec2_div(delta, (float)WINDOW_HEIGHT);
}

static vec2_t get_cursor_pos(window_t *window) {
    float xpos, ypos;
    input_query_cursor(window, &xpos, &ypos);
    return vec2_new(xpos, ypos);
}

static void button_callback(window_t *window, button_t button, int pressed) {
    record_t *record = (record_t*)window_get_userdata(window);
    motion_t *motion = &record->motion;
    vec2_t cursor_pos = get_cursor_pos(window);
    if (button == BUTTON_L) {
        if (pressed) {
            record->orbiting = 1;
            record->orbit_pos = cursor_pos;
        } else {
            vec2_t delta = calculate_delta(record->orbit_pos, cursor_pos);
            record->orbiting = 0;
            motion->orbit = vec2_add(motion->orbit, delta);
        }
    } else if (button == BUTTON_R) {
        if (pressed) {
            record->panning = 1;
            record->pan_pos = cursor_pos;
        } else {
            vec2_t delta = calculate_delta(record->pan_pos, cursor_pos);
            record->panning = 0;
            motion->pan = vec2_add(motion->pan, delta);
        }
    }
}

static void scroll_callback(window_t *window, float offset) {
    record_t *record = (record_t*)window_get_userdata(window);
    motion_t *motion = &record->motion;
    motion->dolly += offset;
}

static void update_camera(window_t *window, camera_t *camera,
                          record_t *record) {
    motion_t *motion = &record->motion;
    vec2_t cursor_pos = get_cursor_pos(window);
    if (record->orbiting) {
        vec2_t delta = calculate_delta(record->orbit_pos, cursor_pos);
        motion->orbit = vec2_add(motion->orbit, delta);
        record->orbit_pos = cursor_pos;
    }
    if (record->panning) {
        vec2_t delta = calculate_delta(record->pan_pos, cursor_pos);
        motion->pan = vec2_add(motion->pan, delta);
        record->pan_pos = cursor_pos;
    }
    if (input_key_pressed(window, KEY_SPACE)) {
        camera_set_transform(camera, CAMERA_POSITION, CAMERA_TARGET);
    } else {
        camera_orbit_update(camera, *motion);
    }
    memset(motion, 0, sizeof(motion_t));
}

static void update_light(window_t *window, float delta_time,
                         record_t *record) {
    if (input_key_pressed(window, KEY_SPACE)) {
        record->light_theta = LIGHT_THETA;
        record->light_phi = LIGHT_PHI;
    } else {
        float angle = LIGHT_SPEED * delta_time;
        if (input_key_pressed(window, KEY_A)) {
            record->light_theta -= angle;
        }
        if (input_key_pressed(window, KEY_D)) {
            record->light_theta += angle;
        }
        if (input_key_pressed(window, KEY_S)) {
            float phi_max = PI - EPSILON;
            record->light_phi += angle;
            if (record->light_phi > phi_max) {
                record->light_phi = phi_max;
            }
        }
        if (input_key_pressed(window, KEY_W)) {
            float phi_min = EPSILON;
            record->light_phi -= angle;
            if (record->light_phi < phi_min) {
                record->light_phi = phi_min;
            }
        }
    }
}

static vec3_t calculate_light(record_t *record) {
    float theta = record->light_theta;
    float phi = record->light_phi;
    float x = (float)sin(phi) * (float)sin(theta);
    float y = (float)cos(phi);
    float z = (float)sin(phi) * (float)cos(theta);
    return vec3_new(-x, -y, -z);
}

void test_helper(tickfunc_t *tickfunc, void *userdata) {
    window_t *window;
    framebuffer_t *framebuffer;
    camera_t *camera;
    record_t record;
    callbacks_t callbacks;
    context_t context;
    float aspect;
    float prev_time;
    float report_time;
    int num_frames;

    window = window_create(WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT);
    framebuffer = framebuffer_create(WINDOW_WIDTH, WINDOW_HEIGHT);
    aspect = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;
    camera = camera_create(CAMERA_POSITION, CAMERA_TARGET, aspect);

    memset(&record, 0, sizeof(record_t));
    record.light_theta = LIGHT_THETA;
    record.light_phi   = LIGHT_PHI;

    memset(&callbacks, 0, sizeof(callbacks_t));
    callbacks.button_callback = button_callback;
    callbacks.scroll_callback = scroll_callback;

    memset(&context, 0, sizeof(context_t));
    context.framebuffer = framebuffer;
    context.camera      = camera;

    window_set_userdata(window, &record);
    input_set_callbacks(window, callbacks);

    num_frames = 0;
    prev_time = input_get_time();
    report_time = prev_time;
    while (!window_should_close(window)) {
        float curr_time = input_get_time();
        float delta_time = curr_time - prev_time;
        prev_time = curr_time;

        update_camera(window, camera, &record);
        update_light(window, delta_time, &record);

        context.light_dir = calculate_light(&record);
        context.delta_time = delta_time;
        tickfunc(&context, userdata);

        window_draw_buffer(window, framebuffer);
        num_frames += 1;
        if (curr_time - report_time >= 1) {
            printf("fps: %d\n", num_frames);
            num_frames = 0;
            report_time = curr_time;
        }

        input_poll_events();
    }

    window_destroy(window);
    framebuffer_release(framebuffer);
    camera_release(camera);
}