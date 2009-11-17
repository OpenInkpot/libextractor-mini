/*
  LibRCD

  Copyright (C) 2005-2008 Suren A. Chilingaryan <csa@dside.dyndns.org>

  This library is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License version 2.1 or later
  as published by the Free Software Foundation.

  This library is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
  for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, write to the Free Software Foundation, Inc.,
  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
*/

#include <stdio.h>
#include <string.h>
#include "tag_id3.h"

#define bit(i) (1<<i)
int check_utf8(const unsigned char *buf, int len) {
    long i,j;
    int bytes=0,rflag=0;
    unsigned char tmp;
    int res=0;

    for (i=0;i<len;i++) {
	if (buf[i]<128) continue;

	if (bytes>0) {
	    if ((buf[i]&0xC0)==0x80) {
		if (rflag) {
		    tmp=buf[i]&0x3F;
		    // Russian is 0x410-0x44F
		    if ((rflag==1)&&(tmp>=0x10)) res++;
		    else if ((rflag==2)&&(tmp<=0x0F)) res++;
		}
		bytes--;
	    } else {
		res--;
		bytes=1-bytes;
		rflag=0;
	    }
	} else {
	    for (j=6;j>=0;j--)
		if ((buf[i]&bit(j))==0) break;

	    if ((j==0)||(j==6)) {
		if ((j==6)&&(bytes<0)) bytes++;
		else res--;
		continue;
	    }
	    bytes=6-j;
	    if (bytes==1) {
		// Cyrrilic D0-D3, Russian - D0-D1
		if (buf[i]==0xD0) rflag=1;
		else if (buf[i]==0xD1) rflag=2;
	    }
	}

	if ((buf[i]==0xD0)||(buf[i]==0xD1)) {
	    if (i+1==len) break;

	}
    }
//    printf("check_utf8: %d : %s\n", res, buf);
    return res > 1;
}

