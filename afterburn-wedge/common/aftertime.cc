/*********************************************************************
 *
 * Copyright (C) 2005, University of Karlsruhe
 *
 * File path:     afterburn-wedge/common/aftertime.cc
 * Description:   Functions to convert between representations of time.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: aftertime.cc,v 1.1 2005/11/05 22:27:24 joshua Exp $
 *
 ********************************************************************/


#include <aftertime.h>

/* Julian dates (JD) are simply a continuous count of days and fractions since
 * noon Universal Time on January 1, 4713 BCE.
 *
 * The Gregorian calendar has a leap year every fourth year except century
 * years not exactly divisible by 400.  The Julian calendar has a leap year
 * every fourth year.  The changeover from the Julian calendar to the Gregorian
 * calendar occurred sometime before 1970.
 *
 * Unix time() value: The number of seconds elapsed since 00:00 Universal time
 * on January 1, 1970 in the Gregorian calendar (Julian day 2440587.5).
 */

word_t gregorian_to_julian_day( word_t year, word_t month, word_t day )
{
    return ( 1461 * ( year + 4800 + ( month - 14 ) / 12 ) ) / 4 +
	( 367 * ( month - 2 - 12 * ( ( month - 14 ) / 12 ) ) ) / 12 -
	( 3 * ( ( year + 4900 + ( month - 14 ) / 12 ) / 100 ) ) / 4 +
	day - 32075;
}

void julian_day_to_gregorian( word_t jd, word_t &year, word_t &month, 
	word_t &day )
{
    word_t l, n, i, j;

    l = jd + 68569;
    n = ( 4 * l ) / 146097;
    l = l - ( 146097 * n + 3 ) / 4;
    i = ( 4000 * ( l + 1 ) ) / 1461001;
    l = l - ( 1461 * i ) / 4 + 31;
    j = ( 80 * l ) / 2447;
    day = l - ( 2447 * j ) / 80;
    l = j / 11;
    month = j + 2 - ( 12 * l );
    year = 100 * ( n - 49 ) + i + l;
}

void unix_to_gregorian( time_t unix_seconds, 
	word_t &year, word_t &month, word_t &day, 
	word_t &hours, word_t &minutes, word_t &seconds )
{
    word_t jd = (unix_seconds + 12*3600) / (24*3600) + 2440587;
    julian_day_to_gregorian( jd, year, month, day );

    seconds = unix_seconds % 60;
    unix_seconds /= 60;
    minutes = unix_seconds % 60;
    unix_seconds /= 60;
    hours = unix_seconds % 24;
}

word_t day_of_week( word_t year, word_t month, word_t day )
    // 0 = Sunday, 1 = Monday, 2 = Tuesday, ...
{
    word_t a = (14 - month) / 12;
    word_t y = year - a;
    word_t m = month + 12*a - 2;
    return (day + y + y/4 - y/100 + y/400 + 31*m/12) % 7;
}

