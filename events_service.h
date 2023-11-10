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

#ifndef EVENTS_SERVICE_H
#define EVENTS_SERVICE_H

int events_get_service_capabilities();
int events_subscribe();
int events_renew();
int events_unsubscribe();
int events_get_event_properties();
int events_set_synchronization_point();

int events_unsupported(const char *method);

#endif //EVENTS_SERVICE_H