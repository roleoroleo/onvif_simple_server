/*
 * Copyright (c) 2026 Oleksandr Kravchuk.
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

#ifndef IMAGING_SERVICE_H
#define IMAGING_SERVICE_H

int imaging_get_service_capabilities();
int imaging_get_imaging_settings();
int imaging_set_imaging_settings();
int imaging_get_options();
int imaging_get_move_options();
int imaging_move();
int imaging_stop();
int imaging_get_status();

int imaging_unsupported(const char *method);

#endif //IMAGING_SERVICE_H
