#ifndef __XBEE_NET_CALLBACKS_H
#define __XBEE_NET_CALLBACKS_H

/*
	libxbee - a C library to aid the use of Digi's XBee wireless modules
	          running in API mode (AP=2).

	Copyright (C) 2009	Attie Grande (attie@attie.co.uk)

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.	If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef XBEE_NO_NET_SERVER

struct xbee_netCallback {
	const char *name;
	const xbee_t_conCallback callback;
};

extern const struct xbee_netCallback xbee_netServerCallbacks[];

#endif /* XBEE_NO_NET_SERVER */

#endif /* __XBEE_NET_CALLBACKS_H */