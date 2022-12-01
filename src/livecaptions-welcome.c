/* livecaptions-window.c
 *
 * Copyright 2022 abb128
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "livecaptions-config.h"
#include "livecaptions-welcome.h"
#include "livecaptions-application.h"
#include "audiocap.h"
#include "common.h"

#include <april_api.h>

void benchmark_dummy_handler(void* userdata, AprilResultType result, size_t count, const AprilToken* tokens){}


gboolean update_progress(gpointer userdata){
    LiveCaptionsWelcome *self = userdata;

    gtk_progress_bar_set_fraction(self->benchmark_progress, self->benchmark_progress_v);

    return G_SOURCE_REMOVE;
}


gboolean benchmark_finish(gpointer userdata){
    LiveCaptionsWelcome *self = userdata;
    printf("Result: %.2f\n", self->benchmark_result_v);

    if(self->benchmark_result_v < MINIMUM_BENCHMARK_RESULT) {
        gtk_stack_set_visible_child(self->stack, self->benchmark_result_bad);
    } else {
        gtk_stack_set_visible_child(self->stack, self->benchmark_result_good);
    }

    return G_SOURCE_REMOVE;
}

void *run_benchmark_thread(void *userdata) {
    LiveCaptionsWelcome *self = userdata;

    const char *model_path = GET_MODEL_PATH();

    AprilASRModel model = asr_thread_get_model(self->application->asr);
    g_assert(model != NULL);

    AprilConfig config = {
        .handler = benchmark_dummy_handler,
        .userdata = NULL,
        .flags = ARPIL_CONFIG_FLAG_SYNCHRONOUS
    };

    AprilASRSession session = aas_create_session(model, config);
    g_assert(session != NULL);

    short noise_data[48000];
    for(int i=0; i<48000; i++){
        noise_data[i] = rand();
    }

    size_t sr = aam_get_sample_rate(model);
    g_assert(sr < 48000);

    time_t begin = time(NULL);


    int idx = 0;
    for(int sec=0; sec<30; sec++){
        aas_feed_pcm16(session, &noise_data[idx], sr);

        idx = (idx + sr) % 48000;
        self->benchmark_progress_v = ((double)sec) / 30.0;


        time_t current = time(NULL);
        if(difftime(current, begin) > 40.0) { // if taking over 40 seconds, no chance
            double speed = ((double)sec) / difftime(current, begin);
            self->benchmark_result_v = speed;
            
            goto end;
        }

        g_idle_add(update_progress, self);
    }

    time_t end = time(NULL);

    double speed = 30.0 / difftime(end, begin);
    self->benchmark_result_v = speed;

end:
    aas_free(session);

    g_idle_add(benchmark_finish, self);

    return NULL;
}

void start_benchmark(LiveCaptionsWelcome *self){
    self->benchmark_thread = g_thread_new("initial-benchmark", run_benchmark_thread, self);

    g_assert(self->benchmark_thread != NULL);
}



G_DEFINE_TYPE(LiveCaptionsWelcome, livecaptions_welcome, ADW_TYPE_APPLICATION_WINDOW)

static void do_benchmark(LiveCaptionsWelcome *self) {
    gtk_stack_set_visible_child(self->stack, self->benching_page);
    start_benchmark(self);
}

static void continue_to_notice(LiveCaptionsWelcome *self) {
    gtk_stack_set_visible_child(self->stack, self->accuracy_page);
}

static void complete(LiveCaptionsWelcome *self) {
    livecaptions_application_finish_setup(self->application, self->benchmark_result_v);
}

static void livecaptions_welcome_class_init (LiveCaptionsWelcomeClass *klass) {
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    gtk_widget_class_set_template_from_resource(widget_class, "/net/sapples/LiveCaptions/livecaptions-welcome.ui");
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWelcome, stack);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWelcome, initial_page);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWelcome, benching_page);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWelcome, benchmark_result_good);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWelcome, accuracy_page);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWelcome, benchmark_result_bad);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWelcome, benchmark_progress);

    gtk_widget_class_bind_template_callback (widget_class, do_benchmark);
    gtk_widget_class_bind_template_callback (widget_class, complete);
    gtk_widget_class_bind_template_callback (widget_class, continue_to_notice);
}


static void livecaptions_welcome_init (LiveCaptionsWelcome *self) {
    gtk_widget_init_template(GTK_WIDGET(self));


}
