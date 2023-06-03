/*
 * Copyright (c) 2023 roleo.
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

#ifndef MEDIA_SERVICE_H
#define MEDIA_SERVICE_H

int media_get_service_capabilities();
int media_get_video_sources();
int media_get_video_source_configurations();
int media_get_video_source_configuration();
int media_get_compatible_video_source_configurations();
int media_get_video_source_configuration_options();
int media_get_profiles();
int media_get_profile(char *input);
int media_get_video_encoder_configurations();
int media_get_video_encoder_configuration(char *input);
int media_get_compatible_video_encoder_configurations(char *input);
int media_get_video_encoder_configuration_options(char *input);
int media_get_snapshot_uri(char *input);
int media_get_stream_uri(char *input);

int media_unsupported(char *action);

#endif //MEDIA_SERVICE_H