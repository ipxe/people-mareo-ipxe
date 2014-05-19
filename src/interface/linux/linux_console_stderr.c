/*
 * Copyright (C) 2014 Marin Hannache <ipxe@mareo.fr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

FILE_LICENCE(GPL2_OR_LATER);

/** @file
 *
 * Linux stderr console implementation.
 *
 */

#include <ipxe/console.h>

#include <linux_api.h>
#include <asm/errno.h>

#include <config/console.h>

/* Set default console usage if applicable */
#if ! ( defined ( CONSOLE_LINUX_STDERR ) && \
        CONSOLE_EXPLICIT ( CONSOLE_LINUX_STDERR ) )
#undef CONSOLE_LINUX_STDERR
#define CONSOLE_LINUX_STDERR ( CONSOLE_USAGE_ALL & ~CONSOLE_USAGE_TUI )
#endif

static int linux_console_stderr_entered;

static void linux_console_stderr_putchar( int c ) {
	if ( linux_console_stderr_entered )
		return;

	linux_console_stderr_entered = 1;

	/* write to stderr */
	if ( linux_write( 2, &c, 1 ) != 1 )
		DBG ( "linux_console_stderr write failed (%s)\n",
		      linux_strerror( linux_errno ) );

	linux_console_stderr_entered = 0;
}

struct console_driver linux_console_stderr __console_driver = {
	.disabled = CONSOLE_DISABLED_INPUT,
	.putchar = linux_console_stderr_putchar,
	.usage = CONSOLE_LINUX_STDERR,
};
