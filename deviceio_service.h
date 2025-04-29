/*
 * Copyright (c) 2024 roleo.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DEVICEIO_SERVICE_H
#define DEVICEIO_SERVICE_H

int deviceio_get_video_sources();
int deviceio_get_service_capabilities();
int deviceio_get_audio_outputs();
int deviceio_get_audio_sources();
int deviceio_get_relay_outputs();
int deviceio_get_relay_output_options();
int deviceio_set_relay_output_settings();
int deviceio_set_relay_output_state();

int deviceio_unsupported(const char *method);

#endif //DEVICEIO_SERVICE_H
