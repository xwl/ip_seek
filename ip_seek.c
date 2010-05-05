/* Extract IP information from QQWry.dat.
   Copyright (C) 2010 William Xu <william.xwl@gmail.com> 

This porgram is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Emacs.  If not, see <http://www.gnu.org/licenses/>.  */

/* Format doc by lumaqq: 
     http://lumaqq.linuxsir.org/article/qqwry_format_detail.html 
*/

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>

#define INDEX_SIZE         7
#define RECORD_OFFSET_SZ   3
#define REDIRECT_OFFSET_SZ 3

#define COUNTRY_SIZE 256
#define AREA_SIZE    256

FILE *data;
int is_big_endian;

inline unsigned int little2big32(unsigned int little)
{
     return ((little & 0xff000000) >> 24) 
          + ((little & 0xff0000) >> 8)
          + ((little & 0xff00) << 8)
          + ((little & 0xff) << 24);
}

inline unsigned int little2big24(unsigned int little)
{
     return ((little & 0xff0000) >> 8)
          + ((little & 0xff00) << 8)
          + ((little & 0xff) << 16);
}

/* Return record offset for IP. */
unsigned int binary_search(unsigned int index_start, 
                           unsigned int index_end,
                           const char *ip)
{
     unsigned int ip_binary = ntohl(inet_addr(ip));
     unsigned int index_base = index_start;
     unsigned int start = 0, end = (index_end - index_start) / INDEX_SIZE;
     unsigned int ip_binary_tmp = 0;
     
     while (start < end){
          unsigned int mid = (start + end) / 2;
          fseek(data, index_base + mid * INDEX_SIZE, SEEK_SET);

          fread(&ip_binary_tmp, sizeof(ip_binary_tmp), 1, data);
          if(is_big_endian)
               ip_binary_tmp = little2big32(ip_binary_tmp);

          /* debug */
          /* struct in_addr in; */
          /* in.s_addr = htonl(ip_binary_tmp); */
          /* printf("%s\n", inet_ntoa(in)); */
          
          if(ip_binary == ip_binary_tmp)
               break;
          else if(ip_binary < ip_binary_tmp)
               end = mid - 1;
          else
               start = mid + 1;
     }

     /* Exact IP may not exist in DATA_FILE, anyhow, try its most adjacent ip. */     
     if(start == end){
          fseek(data, index_base + start * INDEX_SIZE, SEEK_SET);

          fread(&ip_binary_tmp, sizeof(ip_binary_tmp), 1, data);
          if(is_big_endian)
               ip_binary_tmp = little2big32(ip_binary_tmp);
          
          if(ip_binary < ip_binary_tmp)
               fseek(data, index_base + (start - 1) * INDEX_SIZE + 4, SEEK_SET);
     }
     
     unsigned int record_offset = 0;

     fread(&record_offset, RECORD_OFFSET_SZ, 1, data);
     if(is_big_endian)
          record_offset = little2big24(record_offset);

     return record_offset;
}

void read_till_zero(unsigned char *s)
{
     unsigned char c = 0;
     long redirect = 0, restore = 0;

     fread(&c, sizeof(c), 1, data);

     if(c == 1 || c == 2){
          fread(&redirect, REDIRECT_OFFSET_SZ, 1, data);

         if(is_big_endian)
              redirect = little2big24(redirect);

          if(c == 2)
               restore = ftell(data);

          fseek(data, redirect, SEEK_SET);
          read_till_zero(s);
          
          if(c == 2)
               fseek(data, restore, SEEK_SET);
     }
     else
          while(*s ++ = c)
               fread(&c, sizeof(c), 1, data);
}

/* Note: string are encoded in GBK. */
void read_record(unsigned char *country, unsigned char *area)
{
     read_till_zero(country);
     read_till_zero(area);
}


int main(int argc, char *argv[])
{
     if(argc < 3){
          fprintf(stderr, "Usage: %s DATA_FILE IP\n", argv[0]);
          exit(EXIT_FAILURE);
     }

     const char *filename = argv[1];
     const char *ip = argv[2];

     data = fopen(filename, "r");
     if(!data){
          fprintf(stderr, "Error opening file: %s\n", filename);
          exit(EXIT_FAILURE);
     }

     is_big_endian = (htonl(1) == 1);
     
     /* 1. read index offsets */
     unsigned int index_start = 0, index_end = 0;
     
     fread(&index_start, sizeof(index_start), 1, data);
     fread(&index_end,   sizeof(index_end),   1, data);
     
     if(is_big_endian){
          index_start = little2big32(index_start);
          index_end   = little2big32(index_end);
     }
     
     /* 2. find record offset */
     unsigned int record_offset = binary_search(index_start, index_end, ip);

     /* 3. read record */
     unsigned char country[COUNTRY_SIZE] = {'\0'}, area[AREA_SIZE] = {'\0'};

     fseek(data, record_offset + 4, SEEK_SET); /* skip 4 bytes ip */

     read_record(country, area);

     printf("%s", country);
     if(area[0])
          printf(", %s", area);
     printf("\n");
     
     return 0;
}


/* ip_seek.c ends here */
