/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 Bj�rn Stenberg
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

/* ID3 formatting based on code from the MAD Winamp plugin (in_mad.dll), 
 * Copyright (C) 2000-2001 Robert Leslie. 
 * See http://www.mars.org/home/rob/proj/mpeg/ for more information.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lcd.h"
#include "mpeg.h"
#include "id3.h"
#include "settings.h"
#include "playlist.h"
#include "kernel.h"
#include "system.h"
#include "status.h"
#include "wps-display.h"
#include "debug.h"

#ifdef HAVE_LCD_BITMAP
#include "icons.h"
#include "widgets.h"
#endif

#ifdef LOADABLE_FONTS
#include "ajf.h"
#endif

#define WPS_CONFIG ROCKBOX_DIR "/wps.config"

#define MAX_LINES 10
#define FORMAT_BUFFER_SIZE 300

struct format_flags
{
    bool dynamic;
    bool scroll;
#ifdef HAVE_LCD_CHARCELLS
    bool player_progress;
#endif
};

static char format_buffer[FORMAT_BUFFER_SIZE];
static char* format_lines[MAX_LINES];
static bool dynamic_lines[MAX_LINES];
static int ff_rewind_count;
bool wps_time_countup = true;

/* Set format string to use for WPS, splitting it into lines */
static void wps_format(char* fmt)
{
    char* buf = format_buffer;
    int line = 0;
    
    strncpy(format_buffer, fmt, sizeof(format_buffer));
    format_buffer[sizeof(format_buffer) - 1] = 0;
    format_lines[line] = buf;
    
    while (*buf)
    {
        switch (*buf++)
        {
            case '\r':
                *(buf - 1) = 0;
                break;                

            case '\n': /* LF */
                *(buf - 1) = 0;
                line++;
                
                if (line < MAX_LINES)
                {
                    format_lines[line] = buf;
                }
                
                break;
        }
    }

    for (; line < MAX_LINES; line++)
    {
        format_lines[line] = NULL;
    }
}

static bool load_custom_wps(void)
{
    char buffer[FORMAT_BUFFER_SIZE];
    int fd;

    fd = open(WPS_CONFIG, O_RDONLY);
    
    if (-1 != fd)
    {
        int numread = read(fd, buffer, sizeof(buffer) - 1);
        
        if (numread > 0)
        {
            buffer[numread] = 0;
            wps_format(buffer);
        }
        
        close(fd);
        return numread > 0;
    }
    
    return false;
}

/* Format time into buf.
 *
 * buf      - buffer to format to.
 * buf_size - size of buffer.
 * time     - time to format, in milliseconds.
 */
static void format_time(char* buf, int buf_size, int time)
{
    snprintf(buf, buf_size, "%d:%02d", time / 60000, time % 60000 / 1000);
}

/* Extract a part from a path.
 *
 * buf      - buffer extract part to.
 * buf_size - size of buffer.
 * path     - path to extract from.
 * level    - what to extract. 0 is file name, 1 is parent of file, 2 is 
 *            parent of parent, etc.
 *
 * Returns buf if the desired level was found, NULL otherwise.
 */
static char* get_dir(char* buf, int buf_size, char* path, int level)
{
    char* sep;
    char* last_sep;
    int len;

    sep = path + strlen(path);
    last_sep = sep;

    while (sep > path)
    {
        if ('/' == *(--sep))
        {
            if (!level)
            {
                break;
            }
            
            level--;
            last_sep = sep - 1;
        }
    }

    if (level || (last_sep <= sep))
    {
        return NULL;
    }

    len = MIN(last_sep - sep, buf_size - 1);
    strncpy(buf, sep + 1, len);
    buf[len] = 0;
    return buf;
}

/* Get the tag specified by the two characters at fmt.
 *
 * id3      - ID3 data to get tag values from.
 * tag      - string (of two characters) specifying the tag to get.
 * buf      - buffer to certain tags, such as track number, play time or 
 *           directory name.
 * buf_size - size of buffer.
 * flags    - flags in this struct will be set depending on the tag:
 *            dynamic - if the tag data changes over time (like play time);
 *            player_progress - set if the tag is %pb.
 *
 * Returns the tag. NULL indicates the tag wasn't available.
 */
static char* get_tag(struct mp3entry* id3, char* tag, char* buf, int buf_size,
    struct format_flags* flags)
{
    if ((0 == tag[0]) || (0 == tag[1]))
    {
        return NULL;
    }
    
    switch (tag[0])
    {
    case 'i':  /* ID3 Information */
        switch (tag[1])
        {
        case 't':  /* ID3 Title */
            return id3->title;

        case 'a':  /* ID3 Artist */
            return id3->artist;
            
        case 'n':  /* ID3 Track Number */
            if (id3->tracknum)
            {
                snprintf(buf, buf_size, "%d", id3->tracknum);
                return buf;
            }
            else
            {
                return NULL;
            }

        case 'd':  /* ID3 Album/Disc */
            return id3->album;
        }
        break;

    case 'f':  /* File Information */
        switch(tag[1])
        {
        case 'v':  /* VBR file? */
            return id3->vbr ? "(avg)" : NULL;

        case 'b':  /* File Bitrate */
            snprintf(buf, buf_size, "%d", id3->bitrate);
            return buf;

        case 'f':  /* File Frequency */
            snprintf(buf, buf_size, "%d", id3->frequency);
            return buf;

        case 'p':  /* File Path */
            return id3->path;

        case 'm':  /* File Name - With Extension */
            return get_dir(buf, buf_size, id3->path, 0);

        case 'n':  /* File Name */
            if (get_dir(buf, buf_size, id3->path, 0))
            {
                /* Remove extension */
                char* sep = strrchr(buf, '.');

                if (NULL != sep)
                {
                    *sep = 0;
                }

                return buf;
            }
            else
            {
                return NULL;
            }

        case 's':  /* File Size (in kilobytes) */
            snprintf(buf, buf_size, "%d", id3->filesize / 1024);
            return buf;
        }
        break;

    case 'p':  /* Playlist/Song Information */
        switch(tag[1])
        {
#if defined(HAVE_LCD_CHARCELLS) && !defined(SIMULATOR)
        case 'b':  /* Player progress bar */
            flags->player_progress = true;
            flags->dynamic = true;
            return "\x01";
#endif

        case 'p':  /* Playlist Position */
            snprintf(buf, buf_size, "%d", id3->index + 1);
            return buf;

        case 'e':  /* Playlist Total Entries */
            snprintf(buf, buf_size, "%d", playlist.amount);
            return buf;

        case 'c':  /* Current Time in Song */
            flags->dynamic = true;
            format_time(buf, buf_size, id3->elapsed + ff_rewind_count);
            return buf;

        case 'r': /* Remaining Time in Song */
            flags->dynamic = true;
            format_time(buf, buf_size, id3->length - id3->elapsed + ff_rewind_count);
            return buf;

        case 't':  /* Total Time */
            format_time(buf, buf_size, id3->length);
            return buf;
        }
        break;
    
    case 'd': /* Directory path information */
        switch(tag[1])
        {
        case '1':  /* Parent folder */
            return get_dir(buf, buf_size, id3->path, 1);

        case '2':  /* Parent of parent */
            return get_dir(buf, buf_size, id3->path, 2);

        case '3':  /* Parent of parent of parent */
            return get_dir(buf, buf_size, id3->path, 3);
        }
        break;
    }
    
    return NULL;
}

/* Skip to the end of the current %? conditional.
 *
 * fmt     - string to skip it. Should point to somewhere after the leading 
 *           "<" char (and before or at the last ">").
 * to_else - if true, skip to the else part (after the "|", if any), else skip
 *           to the end (the ">").
 *
 * Returns the new position in fmt.
 */
static char* skip_conditional(char* fmt, bool to_else)
{
    int level = 1;

    while (*fmt)
    {
        switch (*fmt++)
        {
            case '%':
                break;
        
            case '|':
                if (to_else && (1 == level))
                    return fmt;
            
                continue;
            
            case '>':
                if (0 == --level) 
                {
                    if (to_else)
                        fmt--;
                
                    return fmt;
                }
                continue;

            default:
                continue;
        }
        
        switch (*fmt++)
        {
            case 0:
            case '%':
            case '|':
            case '<':
            case '>':
                break;
        
            case '?':
                while (*fmt && ('<' != *fmt))
                    fmt++;
            
                if ('<' == *fmt)
                    fmt++;
            
                level++;
                break;
        
            default:
                break;
        }
    }
    
    return fmt;
}

/* Generate the display based on id3 information and format string.
 *
 * buf      - char buffer to write the display to.
 * buf_size - the size of buffer.
 * id3      - the ID3 data to format with.
 * fmt      - format description.
 * flags    - flags in this struct will be set depending on the tag:
 *            dynamic - if the tag data changes over time (like play time);
 *            player_progress - set if the tag is %pb.
 *            scroll - if line scrolling is requested.
 */
static void format_display(char* buf, 
                           int buf_size, 
                           struct mp3entry* id3, 
                           char* fmt, 
                           struct format_flags* flags)
{
    char temp_buf[128];
    char* buf_end = buf + buf_size - 1;   /* Leave room for end null */
    char* value = NULL;
    int level = 0;
    
    while (fmt && *fmt && buf < buf_end)
    {
        switch (*fmt)
        {
            case '%':
                ++fmt;
                break;
        
            case '|':
            case '>':
                if (level > 0) 
                {
                    fmt = skip_conditional(fmt, false);
                    level--;
                    continue;
                }
                /* Else fall through */

            default:
                *buf++ = *fmt++;
                continue;
        }
        
        switch (*fmt)
        {
            case 0:
                *buf++ = '%';
                break;
        
            case 's':
                flags->scroll = true;
                ++fmt;
                break;
        
            case '%':
            case '|':
            case '<':
            case '>':
                *buf++ = *fmt++;
                break;
        
            case '?':
                fmt++;
                value = get_tag(id3, fmt, temp_buf, sizeof(temp_buf), flags);
            
                while (*fmt && ('<' != *fmt))
                    fmt++;
            
                if ('<' == *fmt)
                    fmt++;
            
                /* No value, so skip to else part */
                if (NULL == value)
                    fmt = skip_conditional(fmt, true);

                level++;
                break;
        
            default:
                value = get_tag(id3, fmt, temp_buf, sizeof(temp_buf), flags);
                fmt += 2;
            
                if (value)
                {
                    while (*value && (buf < buf_end))
                        *buf++ = *value++;
                }
        }
    }
    
    *buf = 0;
}

bool wps_refresh(struct mp3entry* id3, int ffwd_offset, bool refresh_all)
{
    char buf[MAX_PATH];
    struct format_flags flags;
    bool scroll_active = false;
    int i;
#ifdef HAVE_LCD_BITMAP
    int bmp_time_line;
#endif

    if (!id3)
    {
        lcd_stop_scroll();
        lcd_clear_display();
        return false;
    }

    ff_rewind_count = ffwd_offset;

#ifdef HAVE_LCD_CHARCELL
    for (i = 0; i < 2; i++)
#else
    for (i = 0; i < MAX_LINES; i++)
#endif
    {
        if ( !format_lines[i] )
            break;

        if (dynamic_lines[i] || refresh_all)
        {
            flags.dynamic = false;
            flags.scroll = false;
#ifdef HAVE_LCD_CHARCELLS
            flags.player_progress = false;
#endif
            format_display(buf, sizeof(buf), id3, format_lines[i], &flags);
            dynamic_lines[i] = flags.dynamic;
            
#if defined(HAVE_LCD_CHARCELLS) && !defined(SIMULATOR)
            if (flags.player_progress)
                draw_player_progress(id3, ff_rewind_count);
#endif

            if (!scroll_active && flags.scroll && !flags.dynamic)
            {
                scroll_active = true;
                lcd_puts_scroll(0, i, buf);
            }
            else
            {
                lcd_puts(0, i, buf);
            }
        }
    }
#ifdef HAVE_LCD_BITMAP
    if (global_settings.statusbar)
        bmp_time_line = 5;
    else
        bmp_time_line = 6;

    format_display(buf, sizeof(buf), id3, "Time: %pc/%pt", &flags);
    lcd_puts(0, bmp_time_line, buf);

    slidebar(0, LCD_HEIGHT-6, LCD_WIDTH, 6, 
             id3->elapsed * 100 / id3->length, Grow_Right);
    lcd_update();
#endif
    return true;
}

void wps_display(struct mp3entry* id3)
{
    int font_height;

#ifdef LOADABLE_FONTS
    unsigned char *font = lcd_getcurrentldfont();
    font_height = ajf_get_fontheight(font);
#else
    font_height = 8;
#endif

    lcd_clear_display();

    if (!id3 && !mpeg_is_playing())
    {
#ifdef HAVE_LCD_CHARCELLS
        lcd_puts(0, 0, "End of list");
        lcd_puts(0, 1, "<Press ON>");
#else
        lcd_puts(0, 2, "<End of song list>");
        lcd_puts(5, 4, "Press ON");
#endif
    }
    else
    {
        static bool wps_loaded = false;

        if (!wps_loaded) {
            load_custom_wps();
            wps_loaded = true;

            if ( !format_buffer[0] ) {
#ifdef HAVE_LCD_BITMAP
                wps_format("%s%fp\n"
                           "%it\n"
                           "%id\n"
                           "%ia\n"
                           "%fb kbit %fv\n"
                           "%ff Hz\n");
#else
                wps_format("%s%pp/%pe: %?ia<%ia - >%?it<%it|%fm>\n"
                           "%pc/%pt\n");
#endif
            }
        }
    }
    wps_refresh(id3, 0, true);
    status_draw();
    lcd_update();
}

#if defined(HAVE_LCD_CHARCELLS) && !defined(SIMULATOR)
bool draw_player_progress(struct mp3entry* id3, int ff_rewwind_count)
{
    if(!id3)
        return(false);
    char player_progressbar[7];
    char binline[36];
    int songpos = 0;
    int i,j;

    memset(binline, 1, sizeof binline);
    memset(player_progressbar, 1, sizeof player_progressbar);
    if(wps_time_countup == false)
        songpos = ((id3->elapsed - ff_rewwind_count) * 36) / id3->length;
    else
        songpos = ((id3->elapsed + ff_rewwind_count) * 36) / id3->length;
    for (i=0; i < songpos; i++)
        binline[i] = 0;

    for (i=0; i<=6; i++) {
        for (j=0;j<5;j++) {
            player_progressbar[i] <<= 1;
            player_progressbar[i] += binline[i*5+j];
        }
    }
    lcd_define_pattern(8,player_progressbar,7);
    return(true);
}
#endif
