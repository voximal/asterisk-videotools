/* pcm2mp4
 * Video bandwith control for mp4
 * Copyright (C) 2006 Sergio Garcia Murillo
 *
 * sergio.garcia@fontventa.com
 * http://sip.fontventa.com
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
 
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char * argv[])
{

  FILE * fd;
  unsigned char c=0;
  int i;
  int j;

	/* Check args */
	if (argc<2)
	{
		printf("audiomark\nusage: audiomark file\n");
		return -1;
	}
 

	if (argc==3)
	{
	  j=atoi(argv[2]);
  }
  else
  j=1;


	/* Open mp4*/
	fd = fopen(argv[1], "w");
  if (fd!=NULL)  
  while (j)
  {
    j--;
    while(1)
    {    
      for (i=0; i<160; i++)
      fputc ( (int)c , fd );
    
      c++;
      if (!c)
      break;
    }
  }
  fclose (fd);
   
	/* End */
	return 0;
}
