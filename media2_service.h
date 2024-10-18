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

#ifndef MEDIA2_SERVICE_H
#define MEDIA2_SERVICE_H

int media2_get_service_capabilities();
int media2_get_profiles();
int media2_get_video_source_configurations();
int media2_get_video_source_configuration_options();
int media2_get_video_encoder_configurations();
int media2_get_video_encoder_configuration_options();
int media2_get_audio_source_configurations();
int media2_get_audio_source_configuration_options();
int media2_get_audio_encoder_configurations();
int media2_get_audio_encoder_configuration_options();
int media2_get_audio_output_configurations();
int media2_get_audio_output_configuration_options();
int media2_get_audio_decoder_configurations();
int media2_get_audio_decoder_configuration_options();
int media2_get_snapshot_uri();
int media2_get_stream_uri();

int media2_set_video_source_configuration();
int media2_set_audio_source_configuration();
int media2_set_video_encoder_configuration();
int media2_set_audio_encoder_configuration();
int media2_set_audio_output_configuration();

int media2_unsupported(const char *method);

#endif //MEDIA2_SERVICE_H