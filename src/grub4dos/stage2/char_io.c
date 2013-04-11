/* char_io.c - basic console input and output */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 1999,2000,2001,2002,2004  Free Software Foundation, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <shared.h>
#include <term.h>

#ifdef SUPPORT_HERCULES
# include <hercules.h>
#endif

#ifdef SUPPORT_SERIAL
# include <serial.h>
#endif

#ifndef STAGE1_5
struct term_entry term_table[] =
  {
    {
      "console",
      0,
      25,
      console_putchar,
      console_checkkey,
      console_getkey,
      console_getxy,
      console_gotoxy,
      console_cls,
      console_setcolorstate,
      console_setcolor,
      console_setcursor,
      0,
      0
    },
#ifdef SUPPORT_SERIAL
    {
      "serial",
      /* A serial device must be initialized.  */
      TERM_NEED_INIT,
      25,
      serial_putchar,
      serial_checkkey,
      serial_getkey,
      serial_getxy,
      serial_gotoxy,
      serial_cls,
      serial_setcolorstate,
      0,
      0,
      0,
      0
    },
#endif /* SUPPORT_SERIAL */
#ifdef SUPPORT_HERCULES
    {
      "hercules",
      0,
      25,
      hercules_putchar,
      console_checkkey,
      console_getkey,
      hercules_getxy,
      hercules_gotoxy,
      hercules_cls,
      hercules_setcolorstate,
      hercules_setcolor,
      hercules_setcursor,
      0,
      0
    },      
#endif /* SUPPORT_HERCULES */
#ifdef SUPPORT_GRAPHICS
    { "graphics",
      TERM_NEED_INIT, /* flags */
      30, /* number of lines */
      graphics_putchar, /* putchar */
      console_checkkey, /* checkkey */
      console_getkey, /* getkey */
      graphics_getxy, /* getxy */
      graphics_gotoxy, /* gotoxy */
      graphics_cls, /* cls */
      graphics_setcolorstate, /* setcolorstate */
      graphics_setcolor, /* setcolor */
      graphics_setcursor, /* nocursor */
      graphics_init, /* initialize */
      graphics_end /* shutdown */
    },
#endif /* SUPPORT_GRAPHICS */
    /* This must be the last entry.  */
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
  };

/* This must be console.  */
struct term_entry *current_term;

int max_lines = 25;
int count_lines = -1;
int use_pager = 1;
#endif

int quit_print;

void
print_error (void)
{
  if (errnum > ERR_NONE && errnum < MAX_ERR_NUM)
#ifndef STAGE1_5
{
    grub_printf ("\nError %u: %s\n", errnum, err_list[errnum]);
}
#else /* STAGE1_5 */
    grub_printf ("Error %u\n", errnum);
#endif /* STAGE1_5 */
}

char *
convert_to_ascii (char *buf, int c,...)
{
  union {
    unsigned long long ll;
    struct {
	unsigned long lo;
	unsigned long hi;
    };
    struct {
	unsigned char l1;
	unsigned char l2;
	unsigned char l3;
	unsigned char l4;
	unsigned char h1;
	unsigned char h2;
	unsigned char h3;
	unsigned char h4;
    };
  } num;
  char *ptr = buf;

  num.hi = *(unsigned long *)((&c) + 2);
  num.lo = *(unsigned long *)((&c) + 1);

  if (c == 'x' || c == 'X')	/* hex */
  {
    do {
	int dig = num.l1 & 0xF;
	*(ptr++) = ((dig > 9) ? dig + c - 33 : '0' + dig);
    } while (num.ll >>= 4);
  }
  else				/* decimal */
  {
    if ((num.h4 & 0x80) && c == 'd')
    {
	//num.ll = (~num.ll) + 1;
	num.ll = - num.ll;
	*(ptr++) = '-';
	buf++;
    }

    do {
	unsigned long H0, H1, L0, L1;

	/* 0x100000000 == 4294967296 */
	/* num.ll == (H1 * 10 + H0) * 0x100000000 + L1 * 10 + L0 */
	H0 = num.hi % 10;
	H1 = num.hi / 10;
	L0 = num.lo % 10;
	L1 = num.lo / 10;
	/* num.ll == H1 * 10 * 0x100000000 + H0 * 0x100000000 + L1 * 10 + L0 */
	/* num.ll == H1 * 10 * 0x100000000 + H0 * 4294967290 + H0 * 6 + L1 * 10 + L0 */
	L0 += H0 * 6;
	L1 += L0 / 10;
	L0 %= 10;
	/* num.ll == H1 * 10 * 0x100000000 + H0 * 4294967290 + L1 * 10 + L0 */
	/* num.ll == (H1 * 0x100000000 + H0 * 429496729 + L1) * 10 + L0 */
	/* quo = (H1 * 0x100000000 + H0 * 429496729 + L1) */
	/* rem = L0 */
	num.hi = H1;
	num.lo = H0;
	num.lo *= 429496729UL;
	num.ll += L1;
	*(ptr++) = '0' + L0;
    } while (num.ll);
  }
  /* reorder to correct direction!! */
  {
    char *ptr1 = ptr - 1;
    char *ptr2 = buf;
    while (ptr1 > ptr2)
      {
	int tmp = *ptr1;
	*ptr1 = *ptr2;
	*ptr2 = tmp;
	ptr1--;
	ptr2++;
      }
  }

  return ptr;
}

void
grub_putstr (const char *str)
{
  while (*str)
    grub_putchar (*str++);
}

#if 0
/* (Patch from Jamey Sharp, 26 Jan 2009)
 * Replace grub_printf calls with grub_sprintf by #define, not asm magic.
 * define in shared.h:
 * 	#define grub_printf(...) grub_sprintf(NULL, __VA_ARGS__)
 */
//static int grub_printf_return_address;
//void
//grub_printf (const char *format, ...)
//{
//	/* sorry! this does not work :-( */
//	//return grub_sprintf (NULL, format,...);
//#if 1
//  asm volatile ("popl %ebp");	/* restore EBP */
//  //asm volatile ("ret");
//  asm volatile ("popl %0" : "=m"(grub_printf_return_address));
//  asm volatile ("pushl $0");	/* buffer = 0 for grub_sprintf */
//#ifdef HAVE_ASM_USCORE
//  asm volatile ("call _grub_sprintf");
//#else
//  asm volatile ("call grub_sprintf");
//#endif
//  asm volatile ("popl %eax");
//  asm volatile ("pushl %0" : : "m"(grub_printf_return_address));
//  asm volatile ("ret");
//#else
//  int *dataptr = (int *)(void *) &format;
//
//  dataptr--;	/* (*dataptr) is return address */
//  grub_printf_return_address = (*dataptr);	/* save return address */
//
//  asm volatile ("leave");	/* restore ESP and EBP */
//  //asm volatile ("ret");
//  asm volatile ("popl %eax");	/* discard return address */
//  asm volatile ("pushl $0");	/* buffer = 0 for grub_sprintf */
//  asm volatile ("call grub_sprintf");
//  asm volatile ("popl %eax");
//  asm volatile ("pushl %0" : : "m"(grub_printf_return_address));
//  asm volatile ("ret");
//#endif
//}
#endif

int
grub_sprintf (char *buffer, const char *format, ...)
{

  /* Call with buffer==NULL, and it will just printf().  */

  /* XXX hohmuth
     ugly hack -- should unify with printf() */
  unsigned long *dataptr = (unsigned long *)(((int *)(void *) &format) + 1);
  //unsigned long *dataptr = (unsigned long *)(((unsigned int *)(void *) &buffer));
  char c, *ptr, str[32];
  char *bp = buffer;
  char pad;
  int width;
  int length;

  //dataptr++;
  //dataptr++;

  while ((c = *(format++)) != 0)
    {
      if (c != '%')
      {
	if (buffer)
	  *bp++ = c; /* putchar(c); */
	else
	{
	  grub_putchar (c);
	  bp++;
	}
      }
      else
      {
	pad = ' ';
	width = 0;
	length = 0;

get_next_c:
	c = *(format++);

find_specifier:
	switch (c)
	  {
	  case 'd': case 'x':	case 'X':  case 'u':
	    {
		unsigned int lo, hi;

		lo = *(dataptr++);
		hi = (length ? (*(dataptr++)) : 0);
		*convert_to_ascii (str, c, lo, hi) = 0;
	    }
	    //dataptr++;
	    width -= grub_strlen (str);
	    if (width > 0)
	      {
		while(width--)
		    if (buffer)
			*bp++ = pad; /* putchar(pad); */
		    else
		    {
			grub_putchar (pad);
			bp++;
		    }
	      }
	    ptr = str;
	    if (buffer)
	    {
		while (*ptr)
			*bp++ = *(ptr++); /* putchar(*(ptr++)); */
	    } else {
		while (*ptr)
		{
			grub_putchar (*(ptr++));
			bp++;
		}
	    }
	    break;

	  case 'c':
	    if (length)
		break;		/* invalid */
	    if (width > 0)
	      {
		while(--width)
		    if (buffer)
			*bp++ = pad; /* putchar(pad); */
		    else
		    {
			grub_putchar (pad);
			bp++;
		    }
	      }
	    if (buffer)
	    {
		*bp++ = (*(char *)(dataptr++)) /*& 0xff*/;
	    } else {
		grub_putchar ((*(char *)(dataptr++)) /*& 0xff*/);
		bp++;
	    }
	    //dataptr++;
	    break;

	  case 's':
	    if (length)
		break;		/* invalid */
	    width -= grub_strlen ((char *) (unsigned int) *(dataptr));
	    if (width > 0)
	      {
		while(width--)
		    if (buffer)
			*bp++ = pad; /* putchar(pad); */
		    else
		    {
			grub_putchar (pad);
			bp++;
		    }
	      }
	    ptr = (char *)(unsigned int) (*(dataptr++));
	    //dataptr++;
	    if (buffer)
	    {
		while ((c = *(ptr++)) != 0)
			*bp++ = c; /* putchar(c); */
	    } else {
		while ((c = *(ptr++)) != 0)
		{
			grub_putchar (c);
			bp++;
		}
	    }
	    break;
	  case 'l':
	    if (length)
		break;		/* invalid */
	    length++;
	    //c = *(format++);	/* should be one of d, x, X, u */
	    goto get_next_c;
	  case '0':
	    if (length)
		break;		/* invalid */
	    pad = '0';
	  case '1' ... '9':
	    if (length)
		break;		/* invalid */
	    width = c - '0';
	    while ((c = *(format++)) >= '0' && c <= '9')
		width = width * 10 + c - '0';
	    goto find_specifier;
	  } /* switch */
       } /* if */
    } /* while */

  if (buffer)
	*bp = 0;
  return bp - buffer;
}


#ifndef STAGE1_5
#include "grub4dos_version.h"

#ifdef GRUB4DOS_VERSION

void
init_page (void)
{
  int i;
  char tmp_buf[128];
  char ch = ' ';

  cls ();

  if (current_term->setcolorstate)
      current_term->setcolorstate (COLOR_STATE_HEADING);

  grub_sprintf (tmp_buf,
		" GRUB4DOS " GRUB4DOS_VERSION ", Mem: %dK/%dM/%ldM, End: %X",
		(unsigned long)saved_mem_lower,
		(unsigned long)(saved_mem_upper >> 10),
		(unsigned long long)(saved_mem_higher >> 10),
		(unsigned int)(((char *) init_free_mem_start) + 256 * sizeof (char *) + config_len));
  for (i = 0; i < 79; i++)
  {
	if (ch)
		ch = tmp_buf[i];
	grub_putchar (ch ? ch : ' ');
  }

  if (current_term->setcolorstate)
      current_term->setcolorstate (COLOR_STATE_STANDARD);
}

#else

void
init_page (void)
{
  cls ();

  grub_printf ("GNU GRUB  version %s  (%dK lower / %dK upper memory)\n",
	  version_string, saved_mem_lower, saved_mem_upper);
}

#endif

/* The number of the history entries.  */
static int num_history = 0;

/* Get the NOth history. If NO is less than zero or greater than or
   equal to NUM_HISTORY, return NULL. Otherwise return a valid string.  */
static char *
get_history (int no)
{
  int j;
  char *p = (char *) HISTORY_BUF;
  if (no < 0 || no >= num_history)
	return 0;
  /* get history NO */
  for (j = 0; j < no; j++)
  {
	p += *(unsigned short *)p;
	if (p > (char *) HISTORY_BUF + MAX_CMDLINE * HISTORY_SIZE)
	{
		num_history = j;
		return 0;
	}
  }

  return p + 2;
}

/* Add CMDLINE to the history buffer.  */
static void
add_history (const char *cmdline, int no)
{
  int j, len;
  char *p = (char *) HISTORY_BUF;
  /* get history NO */
  for (j = 0; j < no; j++)
  {
	p += *(unsigned short *)p;
	if (p > (char *) HISTORY_BUF + MAX_CMDLINE * HISTORY_SIZE)
		return;
  }
  /* get cmdline length */
  len = grub_strlen (cmdline) + 3;
  if (((char *) HISTORY_BUF + MAX_CMDLINE * HISTORY_SIZE) > (p + len))
	grub_memmove (p + len, p, ((char *) HISTORY_BUF + MAX_CMDLINE * HISTORY_SIZE) - (p + len));
  *(unsigned short *)p = len;
  grub_strcpy (p + 2, cmdline);
  if (num_history < 0x7FFFFFFF)
	num_history++;
}

/* XXX: These should be defined in shared.h, but I leave these here,
	until this code is freezed.  */
#define CMDLINE_WIDTH	78
#define CMDLINE_MARGIN	10
/*  
char *prompt;
int maxlen;
int echo_char;
int readline;
*/
struct get_cmdline_arg get_cmdline_str, *p_getcmdline_arg;
static int xpos, lpos, section;

/* The length of PROMPT.  */
static int plen;
/* The length of the command-line.  */
static int llen;
/* The working buffer for the command-line.  */
static char *buf;

static void cl_refresh (int full, int len);
static void cl_backward (int count);
static void cl_forward (int count);
static void cl_insert (const char *str);
static void cl_delete (int count);
static void cl_init (void);
  
/* Move the cursor backward.  */
static void cl_backward (int count)
{
      lpos -= count;
      
      /* If the cursor is in the first section, display the first section
	 instead of the second.  */
      if (section == 1 && plen + lpos < CMDLINE_WIDTH)
	cl_refresh (1, 0);
      else if (xpos - count < 1)
	cl_refresh (1, 0);
      else
	{
	  xpos -= count;

	  if (current_term->flags & TERM_DUMB)
	    {
	      int i;
	      
	      for (i = 0; i < count; i++)
		grub_putchar ('\b');
	    }
	  else
	    gotoxy (xpos, getxy () & 0xFF);
	}
}

/* Move the cursor forward.  */
static void cl_forward (int count)
{
      lpos += count;

      /* If the cursor goes outside, scroll the screen to the right.  */
      if (xpos + count >= CMDLINE_WIDTH)
	cl_refresh (1, 0);
      else
	{
	  xpos += count;

	  if (current_term->flags & TERM_DUMB)
	    {
	      int i;
	      
	      for (i = lpos - count; i < lpos; i++)
		{
		  if (! p_getcmdline_arg->echo_char)
		    grub_putchar (buf[i]);
		  else
		    grub_putchar (p_getcmdline_arg->echo_char);
		}
	    }
	  else
	    gotoxy (xpos, getxy () & 0xFF);
	}
}

/* Refresh the screen. If FULL is true, redraw the full line, otherwise,
   only LEN characters from LPOS.  */
static void cl_refresh (int full, int len)
{
      int i;
      int start;
      int pos = xpos;
      
      if (full)
	{
	  /* Recompute the section number.  */
	  if (lpos + plen < CMDLINE_WIDTH)
	    section = 0;
	  else
	    section = ((lpos + plen - CMDLINE_WIDTH)
		       / (CMDLINE_WIDTH - 1 - CMDLINE_MARGIN) + 1);

	  /* From the start to the end.  */
	  len = CMDLINE_WIDTH;
	  pos = 0;
	  //grub_putchar ('\r');
	  gotoxy (0, (unsigned char) (getxy ()));

	  /* If SECTION is the first section, print the prompt, otherwise,
	     print `<'.  */
	  if (section == 0)
	    {
	      grub_printf ("%s", p_getcmdline_arg->prompt);
	      len -= plen;
	      pos += plen;
	    }
	  else
	    {
	      grub_putchar ('<');
	      len--;
	      pos++;
	    }
	}

      /* Compute the index to start writing BUF and the resulting position
	 on the screen.  */
      if (section == 0)
	{
	  int offset = 0;
	  
	  if (! full)
	    offset = xpos - plen;
	  
	  start = 0;
	  xpos = lpos + plen;
	  start += offset;
	}
      else
	{
	  int offset = 0;
	  
	  if (! full)
	    offset = xpos - 1;
	  
	  start = ((section - 1) * (CMDLINE_WIDTH - 1 - CMDLINE_MARGIN)
		   + CMDLINE_WIDTH - plen - CMDLINE_MARGIN);
	  xpos = lpos + 1 - start;
	  start += offset;
	}

      /* Print BUF. If ECHO_CHAR is not zero, put it instead.  */
      for (i = start; i < start + len && i < llen; i++)
	{
	  if (! p_getcmdline_arg->echo_char)
	    grub_putchar (buf[i]);
	  else
	    grub_putchar (p_getcmdline_arg->echo_char);

	  pos++;
	}
      
      /* Fill up the rest of the line with spaces.  */
      for (; i < start + len; i++)
	{
	  grub_putchar (' ');
	  pos++;
	}
      
      /* If the cursor is at the last position, put `>' or a space,
	 depending on if there are more characters in BUF.  */
      if (pos == CMDLINE_WIDTH)
	{
	  if (start + len < llen)
	    grub_putchar ('>');
	  else
	    grub_putchar (' ');
	  
	  pos++;
	}
      
      /* Back to XPOS.  */
      if (current_term->flags & TERM_DUMB)
	{
	  for (i = 0; i < pos - xpos; i++)
	    grub_putchar ('\b');
	}
      else
	gotoxy (xpos, getxy () & 0xFF);
}

/* Initialize the command-line.  */
static void cl_init (void)
{
      /* Distinguish us from other lines and error messages!  */
      grub_putchar ('\n');

      /* Print full line and set position here.  */
      cl_refresh (1, 0);
}

/* Insert STR to BUF.  */
static void cl_insert (const char *str)
{
      int l = grub_strlen (str);

      if (llen + l < p_getcmdline_arg->maxlen)
	{
	  if (lpos == llen)
	    grub_memmove (buf + lpos, str, l + 1);
	  else
	    {
	      grub_memmove (buf + lpos + l, buf + lpos, llen - lpos + 1);
	      grub_memmove (buf + lpos, str, l);
	    }
	  
	  llen += l;
	  lpos += l;
	  if (xpos + l >= CMDLINE_WIDTH)
	    cl_refresh (1, 0);
	  else if (xpos + l + llen - lpos > CMDLINE_WIDTH)
	    cl_refresh (0, CMDLINE_WIDTH - xpos);
	  else
	    cl_refresh (0, l + llen - lpos);
	}
}

/* Delete COUNT characters in BUF.  */
static void cl_delete (int count)
{
      grub_memmove (buf + lpos, buf + lpos + count, llen - count + 1);
      llen -= count;
      
      if (xpos + llen + count - lpos > CMDLINE_WIDTH)
	cl_refresh (0, CMDLINE_WIDTH - xpos);
      else
	cl_refresh (0, llen + count - lpos);
}

static int
real_get_cmdline (char *cmdline)
{
  /* This is a rather complicated function. So explain the concept.
     
     A command-line consists of ``section''s. A section is a part of the
     line which may be displayed on the screen, but a section is never
     displayed with another section simultaneously.

     Each section is basically 77 or less characters, but the exception
     is the first section, which is 78 or less characters, because the
     starting point is special. See below.

     The first section contains a prompt and a command-line (or the
     first part of a command-line when it is too long to be fit in the
     screen). So, in the first section, the number of command-line
     characters displayed is 78 minus the length of the prompt (or
     less). If the command-line has more characters, `>' is put at the
     position 78 (zero-origin), to inform the user of the hidden
     characters.

     Other sections always have `<' at the first position, since there
     is absolutely a section before each section. If there is a section
     after another section, this section consists of 77 characters and
     `>' at the last position. The last section has 77 or less
     characters and doesn't have `>'.

     Each section other than the last shares some characters with the
     previous section. This region is called ``margin''. If the cursor
     is put at the magin which is shared by the first section and the
     second, the first section is displayed. Otherwise, a displayed
     section is switched to another section, only if the cursor is put
     outside that section.  */

  int c;
  /* The index for the history.  */
  int history = -1;
  
  buf = (char *) CMDLINE_BUF;
  plen = grub_strlen (p_getcmdline_arg->prompt);
  llen = grub_strlen (cmdline);

  if (p_getcmdline_arg->maxlen > MAX_CMDLINE)
    {
      p_getcmdline_arg->maxlen = MAX_CMDLINE;
      if (llen >= MAX_CMDLINE)
	{
	  llen = MAX_CMDLINE - 1;
	  cmdline[MAX_CMDLINE] = 0;
	}
    }
  lpos = llen;
  grub_strcpy (buf, cmdline);

  cl_init ();

  while ((char)(c = /*ASCII_CHAR*/ (getkey ())) != '\n' && (char)c != '\r')
    {
      /* If READLINE is non-zero, handle readline-like key bindings.  */
      if (p_getcmdline_arg->readline)
	{
	  if ((char)c == 9)	/* TAB lists completions */
	      {
		int i;
		/* POS points to the first space after a command.  */
		int pos = 0;
		int ret;
		char *completion_buffer = (char *) COMPLETION_BUF;
		int equal_pos = -1;
		int is_filename;

		/* Find the first word.  */
		while (buf[pos] == ' ')
		  pos++;
		while (buf[pos] && buf[pos] != '=' && buf[pos] != ' ')
		  pos++;

		is_filename = (lpos > pos);

		/* Find the position of the equal character after a
		   command, and replace it with a space.  */
		for (i = pos; buf[i] && buf[i] != ' '; i++)
		  if (buf[i] == '=')
		    {
		      equal_pos = i;
		      buf[i] = ' ';
		      break;
		    }

		/* Find the position of the first character in this word.  */
		for (i = lpos; i > 0; i--)
		{
		    if (buf[i - 1] == ' ' || buf[i - 1] == '=')
		    {
			/* find backslashes immediately before the space */
			for (ret = i - 2; ret >= 0; ret--)
			{
			    if (buf[ret] != '\\')
				break;
			}

			if (! ((i - ret) & 1)) /* the space not backslashed */
				break;
		    }
		}

		/* Invalidate the cache, because the user may exchange
		   removable disks.  */
		buf_drive = -1;

		/* Copy this word to COMPLETION_BUFFER and do the
		   completion.  */
		grub_memmove (completion_buffer, buf + i, lpos - i);
		completion_buffer[lpos - i] = 0;
		ret = print_completions (is_filename, 1);

		if (! is_filename && ret < 0)
		{
			ret = print_completions ((is_filename=1), 1);
		}
		errnum = ERR_NONE;

		if (ret >= 0)
		  {
		    /* Found, so insert COMPLETION_BUFFER.  */
		    cl_insert (completion_buffer + lpos - i);

		    if (ret > 0)
		      {
			/* There are more than one candidates, so print
			   the list.  */
			grub_putchar ('\n');
			print_completions (is_filename, 0);
			errnum = ERR_NONE;
		      }
		  }

		/* Restore the command-line.  */
		if (equal_pos >= 0)
		  buf[equal_pos] = '=';

		if (ret)
		  cl_init ();
	      }
	  else if (c == KEY_HOME/* || (char)c == 1*/)	/* C-a beginning */
		/* Home= 0x4700 for BIOS, 0x0106 for Linux */
	      cl_backward (lpos);
	  else if (c == KEY_END/* || (char)c == 5*/)	/* C-e end */
		/* End= 0x4F00 for BIOS, 0x0168 for Linux */
	      cl_forward (llen - lpos);
	  else if (c == KEY_RIGHT/* || (char)c == 6*/)	/* C-f forward */
		/* Right= 0x4D00 for BIOS, 0x0105 for Linux */
	      {
		if (lpos < llen)
		  cl_forward (1);
	      }
	  else if (c == KEY_LEFT/* || (char)c == 2*/)	/* C-b backward */
		/* Left= 0x4B00 for BIOS, 0x0104 for Linux */
	      {
		if (lpos > 0)
		  cl_backward (1);
	      }
	  else if (c == KEY_UP/* || (char)c == 16*/)	/* C-p previous */
		/* Up= 0x4800 for BIOS, 0x0153 for Linux */
	      {
		char *p;

		if (history < 0)
		  /* Save the working buffer.  */
		  grub_strcpy (cmdline, buf);
		else if (grub_strcmp (get_history (history), buf) != 0)
		  /* If BUF is modified, add it into the history list.  */
		  add_history (buf, history);

		history++;
		p = get_history (history);
		if (! p)
		  {
		    history--;
		  }
		else
		  {

		    grub_strcpy (buf, p);
		    llen = grub_strlen (buf);
		    lpos = llen;
		    cl_refresh (1, 0);
		  }
	      }
	  else if (c == KEY_DOWN/* || (char)c == 14*/)	/* C-n next command */
		/* Down= 0x5000 for BIOS, 0x0152 for Linux */
	      {
		char *p;

		if (history >= 0)
		  {
		    if (grub_strcmp (get_history (history), buf) != 0)
		      /* If BUF is modified, add it into the history list.  */
		      add_history (buf, history);

		    history--;
		    p = get_history (history);
		    if (! p)
		      p = cmdline;

		    grub_strcpy (buf, p);
		    llen = grub_strlen (buf);
		    lpos = llen;
		    cl_refresh (1, 0);
		  }
	      }
	}

      /* ESC, C-d and C-h are always handled. Actually C-d is not
	 functional if READLINE is zero, as the cursor cannot go
	 backward, but that's ok.  */

		/* Ins= 0x5200 for BIOS, 0x014B for Linux */
		/* PgUp= 0x4900 for BIOS, 0x0153 for Linux */
		/* PgDn= 0x5100 for BIOS, 0x0152 for Linux */

	if ((char)c == 27)	/* ESC immediately return 1 */
	  return 1;
	else if (c == KEY_DC/* || (char)c == 4*/)	/* C-d delete */
		/* Del= 0x5300 for BIOS, 0x014A for Linux */
	  {
	    if (lpos != llen)
	      cl_delete (1);
	  }
	else if (c == KEY_BACKSPACE || (char)c == 8)	/* C-h backspace */
		/* Backspace= 0x0E08 for BIOS, 0x0107 for Linux */
	  {
	    if (lpos > 0)
	    {
	      cl_backward (1);
	      cl_delete (1);
	    }
	  }
	else		/* insert printable character into line */
	  {
#ifdef GRUB_UTIL
	    if (c >= ' ' && c <= '~')
	    {
	      char str[2];

	      *(short *)str = (short)c;
	      cl_insert (str);
	    }
#else
	    if ((char)c >= ' ' && (char)c <= '~')
	    {
	      char str[2];

	      str[0] = (char)c;
	      str[1] = 0;
	      cl_insert (str);
	    }
#endif
	  }
    }

  grub_putchar ('\n');

  /* If ECHO_CHAR is NUL, remove the leading spaces.  */
  lpos = 0;
  if (! p_getcmdline_arg->echo_char)
    while (buf[lpos] == ' ')
      lpos++;

  /* Copy the working buffer to CMDLINE.  */
  grub_memmove (cmdline, buf + lpos, llen - lpos + 1);

  /* If the readline-like feature is turned on and CMDLINE is not
     empty, add it into the history list.  */
  if (p_getcmdline_arg->readline && lpos < llen)
    add_history (cmdline, 0);

  return 0;
}

/* Don't use this with a MAXLEN greater than 1600 or so!  The problem
   is that GET_CMDLINE depends on the everything fitting on the screen
   at once.  So, the whole screen is about 2000 characters, minus the
   PROMPT, and space for error and status lines, etc.  MAXLEN must be
   at least 1, and PROMPT and CMDLINE must be valid strings (not NULL
   or zero-length).

   If ECHO_CHAR is nonzero, echo it instead of the typed character. */
int
get_cmdline (struct get_cmdline_arg p_cmdline)
{
  int old_cursor;
  int ret;
  p_getcmdline_arg = &p_cmdline ;
  old_cursor = setcursor (1);
  
  /* Because it is hard to deal with different conditions simultaneously,
     less functional cases are handled here. Assume that TERM_NO_ECHO
     implies TERM_NO_EDIT.  */
  if (current_term->flags & (TERM_NO_ECHO | TERM_NO_EDIT))
    {
      char *p = p_cmdline.cmdline;
      int c;
      
      /* Make sure that MAXLEN is not too large.  */
      if (p_getcmdline_arg->maxlen > MAX_CMDLINE)
		p_getcmdline_arg->maxlen = MAX_CMDLINE;

      /* Print only the prompt. The contents of CMDLINE is simply discarded,
	 even if it is not empty.  */
      grub_printf ("%s", p_getcmdline_arg->prompt);

      /* Gather characters until a newline is gotten.  */
      while ((c = ASCII_CHAR (getkey ())) != '\n' && c != '\r')
	{
	  /* Return immediately if ESC is pressed.  */
	  if (c == 27)
	    {
	      setcursor (old_cursor);
	      return 1;
	    }

	  /* Printable characters are added into CMDLINE.  */
	  if (c >= ' ' && c <= '~')
	    {
	      if (! (current_term->flags & TERM_NO_ECHO))
		grub_putchar (c);

	      /* Preceding space characters must be ignored.  */
	      if (c != ' ' || p != p_cmdline.cmdline)
		*p++ = c;
	    }
	}

      *p = 0;

      if (! (current_term->flags & TERM_NO_ECHO))
	grub_putchar ('\n');

      setcursor (old_cursor);
      return 0;
    }

  /* Complicated features are left to real_get_cmdline.  */
  ret = real_get_cmdline (p_cmdline.cmdline);
  setcursor (old_cursor);
  return ret;
}

// Parse decimal or hexadecimal ASCII input string to 64-bit integer
// input number may have K,M,G,T or k,m,g,t suffix
// if unitshift is 0, the number is plain number.
//  1K=1024, 1M=1048576, 1G=1<<30, 1T=1<<40
// if unitshift is 9, the input number is number of 512-bytes sectors and suffixes means KBytes, MBytes,...
//  1K=2 sectors, 1M=2048 sectors, ...
// unitshift must be in the range 0-63
int
safe_parse_maxint_with_suffix (char **str_ptr, unsigned long long *myint_ptr, int unitshift)
{
  unsigned long long myint = 0;
  //unsigned long long mult = 10;
  char *ptr = *str_ptr;
  char found = 0;
  char negative = 0;

  /*
   *  The decimal numbers can be positive or negative, ranging from
   *  0x8000000000000000(the minimal long long) to 0x7fffffffffffffff(the maximal long long).
   *  The hex numbers are not checked.
   */

  if (*ptr == '-') /* check whether or not the negative sign exists */
    {
      ptr++;
      negative = 1;
    }

  /*
   *  Is this a hex number?
   */
  if (*ptr == '0' && (ptr[1]|32) == 'x') // |32 convert A-Z to lower case, no need to make sure it is really A-Z
  //if (*ptr == '0' && tolower (*(ptr + 1)) == 'x')
  {
      ptr += 2;
      while(1)
      {
      /* A bit tricky. This below makes use of the equivalence:
	 (A <= B && A <= C) <=> ((A - B) <= (C - B))
	 when C > B and A is unsigned.  */
#if 1
        unsigned char digit;
	digit = (unsigned char)(*ptr-'0'); 
	// '0'...'9' become 0...9, 'A'...'F' become 17...22, 'a'...'f' become 49...54
	if (digit > 9) 
	{
	    digit = (digit|32)	// 'A'...'F' become 49...54, 'a'...'f' become 49...54
	            -49;	// 'A'...'F' become 0...5, 'a'...'f' become 0...5
	    if (digit > 5) 
	        break; // end of hexadecimal number
	    digit +=10;
	} // don't have to call tolower function
#else
	unsigned int digit;

	digit = tolower (*ptr) - '0';
	if (digit > 9)
	  {
	    digit -= 'a' - '0';
	    if (mult == 10 || digit > 5)
	      break;
	    digit += 10;
	  }
#endif
	found = 16;
	if ( myint>>(64-4) ) 
	{ // highest digit has already been filled with non-zero, another left shift will overflow
	  errnum = ERR_NUMBER_OVERFLOW;
	  return 0;
	}
	myint = (myint << 4) | digit;
        ptr++;
      }
  }
  else // separated loop for base-16 and base-10 number may be slightly faster
  {
      while (1)
      {
	unsigned char digit;
	digit = (unsigned char)(*ptr-'0'); 
	if (digit>9) 
	    break;
	found = 10;
#if 1
	if ( myint > ((-1ULL>>1)/10) // multiply with 10 will overflow (result > max signed long long)
	  || (myint = myint*10 + digit,
	      // numbers less than (1ULL<<63) are valid (positive or negative).
	      // overflow if bit63 is set, 
	      (long long)myint < 0 
	      // except for (1ULL<<63) which is valid if negative.
	      // (1ULL<<63)*2 truncated to 64 bit is 0
	      && ((myint+myint) || !negative )
	     )
	  )
	{ 
	    errnum = ERR_NUMBER_OVERFLOW;
	    return 0;
	}
#else
	/* we do not check for hex or negative */
	if (mult == 10 && ! negative)
	  /* 0xFFFFFFFFFFFFFFFF == 18446744073709551615ULL */
  //	if ((unsigned)myint > (((unsigned)(MAXINT - digit)) / (unsigned)mult))
	  if (myint > 1844674407370955161ULL ||
	     (myint == 1844674407370955161ULL && digit > 5))
	    {
	      errnum = ERR_NUMBER_OVERFLOW;
	      return 0;
	    }
	myint *= mult;
	myint += digit;
#endif
	  ptr++;
      }
  }
  if (!found)
    {
      errnum = ERR_NUMBER_PARSING;
      return 0;
    }
  else
  {
    unsigned long long myint2,myint3;
    int myshift;
    switch ((*ptr)|32)
    {
      case 'k': myshift = 10-unitshift; ++ptr; break;
      case 'm': myshift = 20-unitshift; ++ptr; break;
      case 'g': myshift = 30-unitshift; ++ptr; break;
      case 't': myshift = 40-unitshift; ++ptr; break;
      default:  myshift = 0;
    }
    if (myshift >= 0)
	myint3 = (myint2 = myint <<  myshift) >>  myshift;
    else
        myint3 = (myint2 = myint >> -myshift) << -myshift; 
    // myint3 should be equal to myint unless some bit were lost
    if( myint3 != myint // if some bits were lost
      || ((long long)(myint2 ^ myint)<0 && found==10) ) // or sign bit changed for decimal number
    {   
	errnum = ERR_NUMBER_OVERFLOW;
	return 0;
    }
    *myint_ptr = negative? -myint2: myint2;
    *str_ptr = ptr;
    return 1;
  }
}
#if 0
int
safe_parse_maxint (char **str_ptr, unsigned long long *myint_ptr)
{
  char *ptr = *str_ptr;
  unsigned long long myint = 0;
  unsigned long long mult = 10;
  int found = 0;
  int negative = 0;

  /*
   *  The decimal numbers can be positive or negative, ranging from
   *  0x80000000(the minimal int) to 0x7fffffff(the maximal int).
   *  The hex numbers are not checked.
   */

  if (*ptr == '-') /* check whether or not the negative sign exists */
    {
      ptr++;
      negative = 1;
    }

  /*
   *  Is this a hex number?
   */
  if (*ptr == '0' && tolower (*(ptr + 1)) == 'x')
    {
      ptr += 2;
      mult = 16;
    }

  while (1)
    {
      /* A bit tricky. This below makes use of the equivalence:
	 (A >= B && A <= C) <=> ((A - B) <= (C - B))
	 when C > B and A is unsigned.  */
      unsigned int digit;

      digit = tolower (*ptr) - '0';
      if (digit > 9)
	{
	  digit -= 'a' - '0';
	  if (mult == 10 || digit > 5)
	    break;
	  digit += 10;
	}

      found = 1;
      /* we do not check for hex or negative */
      if (mult == 10 && ! negative)
	/* 0xFFFFFFFFFFFFFFFF == 18446744073709551615ULL */
//	if ((unsigned)myint > (((unsigned)(MAXINT - digit)) / (unsigned)mult))
	if (myint > 1844674407370955161ULL ||
	   (myint == 1844674407370955161ULL && digit > 5))
	  {
	    errnum = ERR_NUMBER_OVERFLOW;
	    return 0;
	  }
      myint *= mult;
      myint += digit;
      ptr++;
    }

  if (!found)
    {
      errnum = ERR_NUMBER_PARSING;
      return 0;
    }

  *str_ptr = ptr;
  *myint_ptr = negative ? -myint : myint;

  return 1;
}
#endif
#endif /* STAGE1_5 */

int
grub_tolower (int c)
{
  if (c >= 'A' && c <= 'Z')
    return (c + ('a' - 'A'));

  return c;
}

int
grub_isspace (int c)
{
  switch (c)
    {
    case ' ':
    case '\t':
    case '\r':
    case '\n':
      return 1;
    default:
      break;
    }

  return 0;
}

int
grub_memcmp (const char *s1, const char *s2, int n)
{
  while (n)
    {
      if (*s1 < *s2)
	return -1;
      else if (*s1 > *s2)
	return 1;
      s1++;
      s2++;
      n--;
    }

  return 0;
}

#ifndef STAGE1_5
int
grub_strncat (char *s1, const char *s2, int n)
{
  int i = -1;

  while (++i < n && s1[i] != 0);

  while (i < n && (s1[i++] = *(s2++)) != 0);

  s1[n - 1] = 0;

  if (i >= n)
    return 0;

  s1[i] = 0;

  return 1;
}
#endif /* ! STAGE1_5 */

/* XXX: This below is an evil hack. Certainly, we should change the
   strategy to determine what should be defined and what shouldn't be
   defined for each image. For example, it would be better to create
   a static library supporting minimal standard C functions and link
   each image with the library. Complicated things should be left to
   computer, definitely. -okuji  */
#if !defined(STAGE1_5) || defined(FSYS_VSTAFS)
int
grub_strcmp (const char *s1, const char *s2)
{
  while (*s1 || *s2)
    {
      if (*s1 < *s2)
	return -1;
      else if (*s1 > *s2)
	return 1;
      s1 ++;
      s2 ++;
    }

  return 0;
}
#endif /* ! STAGE1_5 || FSYS_VSTAFS */

#ifndef STAGE1_5
/* Wait for a keypress and return its code.  */
int
getkey (void)
{
  return current_term->getkey ();
}

/* Check if a key code is available.  */
int
checkkey (void)
{
  return current_term->checkkey ();
}
#endif /* ! STAGE1_5 */

/* Display an ASCII character.  */
void
grub_putchar (int c)
{
  /* if it is a Line Feed, we insert a Carriage Return. */

#ifdef STAGE1_5

  /* In Stage 1.5, only the normal console is supported.  */
  
  if (c == '\n')
    //grub_putchar ('\r');	/* recursive, bad!! */
    console_putchar ('\r');
  console_putchar (c);
  
#else /* ! STAGE1_5 */
  
  if (c == '\t' && current_term->getxy)
    {
      c = 8 - ((current_term->getxy () >> 8) & 3);
      while (c--)
	//grub_putchar (' ');	/* recursive, bad!! */
	current_term->putchar (' ');
      return;
    }

  if (c == '\n')
    {
      current_term->putchar ('\r');
      
      /* Internal `more'-like feature.  */
      if (count_lines >= 0)
	{
	  count_lines++;
	  if (count_lines >= max_lines - 2)
	    {
	      /* It's important to disable the feature temporarily, because
		 the following grub_printf call will print newlines.  */
	      count_lines = -1;

	      //grub_printf("\n");	/* recursive, bad!! */
	      current_term->putchar ('\n');
	      
	      if (! (current_term->flags & TERM_DUMB))
	      {
		if (current_term->setcolorstate)
		  current_term->setcolorstate (COLOR_STATE_HIGHLIGHT);
	      
		//grub_printf ("[Hit return to continue]");	/* recursive, bad!! */
		c = (int)"[Hit Q to quit, any other key to continue]";
		while (*(char *)c)
		  current_term->putchar (*(char *)c++);

		if ((getkey () & 0xDF) == 0x51)	/* 0x51 == 'Q' */
			quit_print = 1;
//		do
//		{
//		  tmp = ASCII_CHAR (getkey ());
//		  if ((tmp & 0xdf) == 0x51)	/* Q */
//		  {
//			quit_print = 1;
//			break;
//		  }
//		}
//		while (tmp != '\n' && tmp != '\r');
	      
		if (current_term->setcolorstate)
		  current_term->setcolorstate (COLOR_STATE_STANDARD);

		//grub_printf ("\r                                          \r");	/* recursive, bad!! */
		c = (int)"\r                                          \r";
		while (*(char *)c)
		  current_term->putchar (*(char *)c++);
	      }
	      
	      /* Restart to count lines.  */
	      count_lines = 0;
	      return;
	    }
	}
    }

//  if (! (current_term->flags & TERM_DUMB))
//  while (checkkey () != -1)
//  {
//	if ((getkey () & 0xdf) == 'Q')
//	{
//		quit_print = 1;
//		break;
//	}
//  }
  
  current_term->putchar (c);
  
#endif /* ! STAGE1_5 */
}

#ifndef STAGE1_5
void
gotoxy (int x, int y)
{
  current_term->gotoxy (x, y);
}

int
getxy (void)
{
  return current_term->getxy ();
}

void
cls (void)
{
  /* If the terminal is dumb, there is no way to clean the terminal.  */
  if (current_term->flags & TERM_DUMB)
    grub_putchar ('\n');
  else
    current_term->cls ();
}

int
setcursor (int on)
{
  if (current_term->setcursor)
    return current_term->setcursor (on);

  return 1;
}
#endif /* ! STAGE1_5 */

int
substring (const char *s1, const char *s2, int case_insensitive)
{
  char ch1, ch2;
  
  for (;;)
    {
      ch1 = *(s1++);
      ch2 = *(s2++);
      
      if (case_insensitive)
      {
	ch1 = tolower(ch1);
	ch2 = tolower(ch2);
      }

      if (! ch1)	/* S1 is a substring of S2, or they match exactly */
	return ch2 ? -1 : 0;
      
      if (ch1 != ch2)
	return 1;	/* S1 isn't a substring of S2 */
    }
}

/* Terminate the string STR with NUL.  */
int
nul_terminate (char *str)
{
  int ch;
  
//  while (*str && ! grub_isspace (*str))
//    str++;

  while ((ch = *str) && ! grub_isspace (ch))
  {
	if (ch == '\\')
	{
		str++;
		if (! (ch = *str))
			break;
	}
	str++;
  }

//  ch = *str;
  *str = 0;
  return ch;
}

#ifndef STAGE1_5
char *
grub_strstr (const char *s1, const char *s2)
{
  while (*s1)
    {
      const char *ptr, *tmp;

      ptr = s1;
      tmp = s2;

      while (*tmp && *ptr == *tmp)
	ptr++, tmp++;
      
      if (tmp > s2 && ! *tmp)
	return (char *) s1;

      s1++;
    }

  return 0;
}
#endif /* ! STAGE1_5 */

int
grub_strlen (const char *str)
{
  int len = 0;

  while (*str++)
    len++;

  return len;
}

int
memcheck (unsigned long long addr, unsigned long long len)
{
  errnum = 0;
#ifdef GRUB_UTIL
  if (! addr)
    errnum = ERR_WONT_FIT;
#else
  if (! addr || (! is64bit && (addr >= 0x100000000ULL || addr + len > 0x100000000ULL)))
    errnum = ERR_WONT_FIT;
#endif /* GRUB_UTIL */

  return ! errnum;
}

#if 0
void
grub_memcpy(void *dest, const void *src, int len)
{
  int i;
  register char *d = (char*)dest, *s = (char*)src;

  for (i = 0; i < len; i++)
    d[i] = s[i];
}
#endif


static inline void * _memcpy_forward(void *dst, const void *src, unsigned int len)
{
    int r0, r1, r2, r3;
    __asm__ __volatile__(
	"movl %%ecx, %0; shrl $2, %%ecx; "	// ECX=(len / 4)
	"rep; movsl; "
	"movl %0, %%ecx; andl $3, %%ecx; "	// ECX=(len % 4)
	"rep; movsb; "
	: "=&r"(r0), "=&c"(r1), "=&D"(r2), "=&S"(r3)
	: "1"(len), "2"((long)dst), "3"((long)src)
	: "memory");
    return dst;
}
static inline void * _memcpy_backward(void *dst, const void *src, unsigned int len)
{
    int r0, r1, r2, r3;
    __asm__ __volatile__(
	"std; \n\t"
	"movl %%ecx, %0; andl $3, %%ecx; "	// now ESI,EDI point to end-1, ECX=(len % 4)
	"rep; movsb; "
	"subl $3, %%edi; subl $3, %%esi; "
	"movl %0, %%ecx; shrl $2, %%ecx; "	// now ESI,EDI point to end-4, ECX=(len / 4)
	"rep; movsl; \n\t"
	"cld; \n\t"
	: "=&r"(r0),"=&c"(r1), "=&D"(r2), "=&S"(r3)
	: "1"(len), "2"((long)dst+len-1), "3"((long)src+len-1)
	: "memory");
    return dst;
}
static inline int _memcmp(const void *str1, const void *str2, unsigned int len)
{
    int a, r1, r2, r3;
    __asm__ __volatile__(
	"movl %%ecx, %%eax; shrl $2, %%ecx; "	// ECX=len/4
	"repe; cmpsl; "
	"je   1f; "	// jump if n/4==0 or all longs are equal
	"movl $4, %%ecx; subl %%ecx,%%edi; subl %%ecx,%%esi; " // not equal, compare the previous 4 bytes again byte-by-byte
	"jmp  2f;"
	"\n1:\t"	// all longs are equal, compare the remaining 0-3 bytes
	"andl $3, %%eax; movl %%eax, %%ecx; "	// ECX=len%4
	"\n2:\t"
	"repe; cmpsb; "	// final comparison
	"seta %%al; "	// AL = (str1>str2)? 1:0, 3 high byte of EAX is already 0
	"setb %%cl; "	// CL = (str1<str2)? 1:0, 3 high byte of ECX is already 0
	"subl %%ecx,%%eax; "	// if (str1<str2) EAX = -1
	: "=&a"(a), "=&c"(r1), "=&S"(r2), "=&D"(r3)
	: "1"(len), "2"((long)str1), "3"((long)str2)
	: "memory");
    return a;
}
static inline void _memset(void *dst, unsigned char data, unsigned int len)
{
    int r0,r1,r2,r3;
    __asm__ __volatile__ (
	"movb %b2, %h2; movzwl %w2, %3; shll $16, %2; orl %3, %2; "	// duplicate data into all 4-bytes of EAX
	"movl %0, %3; shrl $2, %0; "	// ECX=(len / 4)
	"rep; stosl;  "
	"movl %3, %0; andl $3, %0; "	// ECX=(len % 4)
	"rep; stosb;  "
	:"=&c"(r1),"=&D"(r2),"=&a"(r0),"=&r"(r3)
	:"0"(len),"1"(dst),"2"(data)
	:"memory");
}


/* struct copy needs the memcpy function */
/* #undef memcpy */
#if 1
void * grub_memcpy(void * to, const void * from, unsigned int n)
{
       /* This assembly code is stolen from
	* linux-2.4.22/include/asm-i386/string.h
	* It assumes ds=es=data space, this should be normal.
	*/
	int d0, d1, d2;
	__asm__ __volatile__(
		"rep ; movsl\n\t"
		"testb $2,%b4\n\t"
		"je 1f\n\t"
		"movsw\n"
		"1:\ttestb $1,%b4\n\t"
		"je 2f\n\t"
		"movsb\n"
		"2:"
		: "=&c" (d0), "=&D" (d1), "=&S" (d2)
		:"0" (n/4), "q" (n),"1" ((long) to),"2" ((long) from)
		: "memory");
	return to;
}
#else
/* just in case the assembly version of grub_memcpy does not work. */
void * grub_memcpy(void * to, const void * from, unsigned int n)
{
	char *d = (char *)to, *s = (char *)from;

	while (n--)
		*d++ = *s++;

	return to;
}
#endif

void *
grub_memmove (void *to, const void *from, int len)
{
   if (memcheck ((int) to, len))
     {
       /* This assembly code is stolen from
	  linux-2.2.2/include/asm-i386/string.h. This is not very fast
	  but compact.  */
       int d0, d1, d2;

       if (to < from)
	 {
	   asm volatile ("cld\n\t"
			 "rep\n\t"
			 "movsb"
			 : "=&c" (d0), "=&S" (d1), "=&D" (d2)
			 : "0" (len),"1" (from),"2" (to)
			 : "memory");
	 }
       else
	 {
	   asm volatile ("std\n\t"
			 "rep\n\t"
			 "movsb\n\t"
			 "cld"
			 : "=&c" (d0), "=&S" (d1), "=&D" (d2)
			 : "0" (len),
			 "1" (len - 1 + (const char *) from),
			 "2" (len - 1 + (char *) to)
			 : "memory");
	 }
     }

   return errnum ? NULL : to;
}

void *
grub_memset (void *start, int c, int len)
{
  char *p = start;

  if (memcheck ((unsigned int)start, len))
    {
      while (len -- > 0)
	*p ++ = c;
    }

  return errnum ? NULL : start;
}

#ifndef STAGE1_5
char *
grub_strcpy (char *dest, const char *src)
{
  grub_memmove (dest, src, grub_strlen (src) + 1);
  return dest;
}
#endif /* ! STAGE1_5 */

#ifndef STAGE1_5
/* The strtok.c comes from reactos. It follows GPLv2. */

/* Copyright (C) 1994 DJ Delorie, see COPYING.DJ for details */
char*
grub_strtok(char *s, const char *delim)
{
  const char *spanp;
  int c, sc;
  char *tok;
  static char *last;
   
  if (s == NULL && (s = last) == NULL)
    return (NULL);

  /*
   * Skip (span) leading delimiters (s += strspn(s, delim), sort of).
   */
 cont:
  c = *s++;
  for (spanp = delim; (sc = *spanp++) != 0;) {
    if (c == sc)
      goto cont;
  }

  if (c == 0) {			/* no non-delimiter characters */
    last = NULL;
    return (NULL);
  }
  tok = s - 1;

  /*
   * Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
   * Note that delim must have one NUL; we stop if we see that, too.
   */
  for (;;) {
    c = *s++;
    spanp = delim;
    do {
      if ((sc = *spanp++) == c) {
	if (c == 0)
	  s = NULL;
	else
	  s[-1] = 0;
	last = s;
	return (tok);
      }
    } while (sc != 0);
  }
  /* NOTREACHED */
}
#endif /* ! STAGE1_5 */

#ifndef GRUB_UTIL
# undef memcpy
/* GCC emits references to memcpy() for struct copies etc.  */
void *memcpy (void *dest, const void *src, int n)  __attribute__ ((alias ("grub_memmove")));
#endif

#if 0
int
grub_memcmp64_lm (const unsigned long long s1, const unsigned long long s2, unsigned long long n)
{
#if !defined(STAGE1_5) && !defined(GRUB_UTIL)
	if (((unsigned long *)&s1)[1] || ((unsigned long *)&s2)[1] || ((unsigned long *)&n)[1] || (s1 + n) > 0x100000000ULL || (s2 + n) > 0x100000000ULL)
	{
		return mem64 (2, s1, s2, n);	/* 2 for CMP */
	}
#endif /* ! STAGE1_5 && ! GRUB_UTIL */

	return grub_memcmp ((char *)(unsigned int)s1, (char *)(unsigned int)s2, n);
}

void
grub_memmove64 (unsigned long long to, const unsigned long long from, unsigned long long len)
{
#if !defined(STAGE1_5) && !defined(GRUB_UTIL)
	if (((unsigned long *)&to)[1] || ((unsigned long *)&from)[1] || ((unsigned long *)&len)[1] || (to + len) > 0x100000000ULL || (from + len) > 0x100000000ULL)
	{
	}
#endif /* ! STAGE1_5 && ! GRUB_UTIL */

	grub_memmove ((void *)(unsigned int)to, (void *)(unsigned int)from, len);
}

void
grub_memset64 (unsigned long long start, unsigned long long c, unsigned long long len)
{
#if !defined(STAGE1_5) && !defined(GRUB_UTIL)
	if (((unsigned long *)&start)[1] || ((unsigned long *)&len)[1] || (start + len) > 0x100000000ULL)
	{
		mem64 (3, start, c, len);	/* 3 for SET */
		return;
	}
#endif /* ! STAGE1_5 && ! GRUB_UTIL */

	grub_memset ((void *)(unsigned int)start, c, len);
}

#endif

#if !defined(STAGE1_5) && !defined(GRUB_UTIL)

#define PAGING_PML4_ADDR (PAGING_TABLES_BUF+0x0000)
#define PAGING_PDPT_ADDR (PAGING_TABLES_BUF+0x1000)
#define PAGING_PD_ADDR   (PAGING_TABLES_BUF+0x2000)

// If this value is changed, memory_paging_map_for_transfer must also be modified.
#define PAGINGTXSTEP 0x800000

#define DST_VIRTUAL_BASE  0x1000000UL
#define SRC_VIRTUAL_BASE  0x2000000UL
#define DST_VIRTUAL_ADDR(addr) (((unsigned long)(addr) & 0x1FFFFFUL)+DST_VIRTUAL_BASE)
#define SRC_VIRTUAL_ADDR(addr) (((unsigned long)(addr) & 0x1FFFFFUL)+SRC_VIRTUAL_BASE)
#define DST_VIRTUAL_PTR(addr) ((void*)DST_VIRTUAL_ADDR(addr))
#define SRC_VIRTUAL_PTR(addr) ((void*)SRC_VIRTUAL_ADDR(addr))

// Set to 0 to test mem64 function
#define DISABLE_AMD64 1

extern void memory_paging_init(void);
extern void memory_paging_enable(void);
extern void memory_paging_disable(void);
extern void memory_paging_map_for_transfer(unsigned long long dst_addr, unsigned long long src_addr);

unsigned char memory_paging_initialized = 0;

void memory_paging_init()
{
    // prepare PDP, PDT
    unsigned long long *paging_PML4 = (unsigned long long *)PAGING_PML4_ADDR;
    unsigned long long *paging_PDPT = (unsigned long long *)PAGING_PDPT_ADDR;
    unsigned long long *paging_PD   = (unsigned long long *)PAGING_PD_ADDR;
    unsigned long long a;
    
    paging_PML4[0] = PAGING_PDPT_ADDR | PML4E_P;
    _memset(paging_PML4+1,0,4096-8*1);
    
    paging_PDPT[0] = PAGING_PD_ADDR | PDPTE_P;
    _memset(paging_PDPT+1,0,4096-8*1);
    
    // virtual address 0-16MB = physical address 0-16MB
    paging_PD[ 0] = a = 0ULL | (PDE_P|PDE_RW|PDE_US|PDE_PS|PDE_G); 
    paging_PD[ 1] = (a += 0x200000); 
    paging_PD[ 2] = (a += 0x200000); 
    paging_PD[ 3] = (a += 0x200000); 
    paging_PD[ 4] = (a += 0x200000); 
    paging_PD[ 5] = (a += 0x200000); 
    paging_PD[ 6] = (a += 0x200000); 
    paging_PD[ 7] = (a += 0x200000); 
    _memset(paging_PD+8,0,4096-8*8);
    
    memory_paging_initialized = 1;
}
void memory_paging_map_for_transfer(unsigned long long dst_addr, unsigned long long src_addr)
{
    unsigned long long *paging_PD = (unsigned long long *)PAGING_PD_ADDR;
    unsigned long long a;
    if (!memory_paging_initialized) 
	memory_paging_init();
    // map 5 2MB-pages for transfer up to 4*2MB.
    // virtual address 16MB 0x01000000
    paging_PD[ 8] = a = (dst_addr&(-2ULL<<20)) | (PDE_P|PDE_RW|PDE_US|PDE_PS); 
    paging_PD[ 9] = (a += 0x200000); 
    paging_PD[10] = (a += 0x200000); 
    paging_PD[11] = (a += 0x200000); 
    paging_PD[12] = (a += 0x200000); 
    // virtual address 32MB 0x02000000
    paging_PD[16] = a = (src_addr&(-2ULL<<20)) | (PDE_P|PDE_RW|PDE_US|PDE_PS); 
    paging_PD[17] = (a += 0x200000); 
    paging_PD[18] = (a += 0x200000); 
    paging_PD[19] = (a += 0x200000); 
    paging_PD[20] = (a += 0x200000); 
    // invalidate non-global TLB entries
    { int r0; 
      asm volatile ("movl %%cr3,%0; movl %0,%%cr3" : "=&q"(r0) : : "memory");
    }
}
void memory_paging_enable()
{
    if (!memory_paging_initialized) 
	memory_paging_init();
    // enable paging
    {
      int r0,r1;
      asm volatile (
	"movl %%cr0, %0; movl %%cr4, %1; \n\t"
	"orl  $0x80000001,%0; \n\t" // CR0.PE(bit0)|PG(bit31)
	"orl  $0x00000030,%1; \n\t" // CR4.PAE(bit4)|PSE(bit5)
	"movl %1, %%cr4; \n\t"  // set PAE|PSE
	"movl %2, %%cr3; \n\t"  // point to PDPT
	"movl %0, %%cr0; \n\t"  // set PE|PG
	"ljmp %3,$(1f) \n1:\t"  // flush instruction cache
	//"movl %4, %0; movl %0, %%ds; movl %0, %%es; movl %0, %%ss;" // reload DS,ES,SS
	"btsl $7, %1;    \n\t"  // CR4.PGE(bit7)
	"movl %1, %%cr4; \n\t"  // set PGE
	:"=&r"(r0),"=&r"(r1)
	:"r"(PAGING_PDPT_ADDR),
	 "i"(PROT_MODE_CSEG),
	 "i"(PROT_MODE_DSEG)
	:"memory");
    }
}
void memory_paging_disable()
{
    int r0;
    asm volatile (
	"movl %%cr0,%0;  andl %1,%0;  movl %0,%%cr0; \n\t"
	"ljmp %3,$(1f) \n1:\t"  // flush instruction cache
	"movl %%cr4,%0;  andl %2,%0;  movl %0,%%cr4; \n\t"
	"                xorl %0,%0;  movl %0,%%cr3; \n\t"
	:"=&a"(r0)
	:"i"(~(CR0_PG)),
	 "i"(~(CR4_PSE|CR4_PAE|CR4_PGE)),
	 "i"(PROT_MODE_CSEG)
	:"memory");
}

#endif /* ! STAGE1_5 && ! GRUB_UTIL */

/*
Transfer data in memory.
Limitation:
code must be below 16MB as mapped by memory_paging_init function
*/
unsigned long long
grub_memmove64(unsigned long long dst_addr, unsigned long long src_addr, unsigned long long len)
{
    if (!len)      { errnum = 0; return dst_addr; }
    if (!dst_addr) { errnum = ERR_WONT_FIT; return 0; }

    // forward copy should be faster than backward copy
    // If src_addr < dst_addr < src_addr+len, forward copy is not safe, so we do backward copy in that case. 
    unsigned char backward = ((src_addr < dst_addr) && (dst_addr < src_addr+len));  
    unsigned long highaddr = (unsigned long)( dst_addr       >>32)
			   | (unsigned long)((dst_addr+len-1)>>32)
			   | (unsigned long)( src_addr       >>32)
			   | (unsigned long)((src_addr+len-1)>>32);
    if ( highaddr==0 )
    { // below 4GB just copy it normally
	void *pdst = (void*)(unsigned long)dst_addr; 
	void *psrc = (void*)(unsigned long)src_addr; 
	if (backward)
	    _memcpy_backward(pdst, psrc, len);
	else
	    _memcpy_forward(pdst, psrc, len);
	errnum = 0; return dst_addr;
    }
#if !defined(STAGE1_5) && !defined(GRUB_UTIL)
    else if ( (highaddr>>(52-32))==0 && (is64bit & IS64BIT_AMD64) && !DISABLE_AMD64)
    { // AMD64/IA32-e paging
	mem64 (1, dst_addr, src_addr, len);	/* 1 for MOVE */
	return dst_addr;
    }
    else if ( (highaddr>>(52-32))==0 && (is64bit & IS64BIT_PAE))
    { // PAE paging
	void *pdst = DST_VIRTUAL_PTR(dst_addr); 
	void *psrc = SRC_VIRTUAL_PTR(src_addr); 
	unsigned long long dsta = dst_addr, srca = src_addr;
	memory_paging_enable();
	memory_paging_map_for_transfer(dsta, srca);
	// transfer
	unsigned long long nr = len; // number of bytes remaining
	while (1)
	{
	    unsigned long n1 = (nr>=PAGINGTXSTEP)? PAGINGTXSTEP: (unsigned long)nr;  // number of bytes per round (8MB)
	    // Copy
	    if (backward)
		_memcpy_backward(pdst, psrc, n1);
	    else
		_memcpy_forward(pdst, psrc, n1);
	    // update loop variables
	    if ((nr -= n1)==0) break;
	    memory_paging_map_for_transfer((dsta+=n1), (srca+=n1));
	}
	memory_paging_disable();
	errnum = 0; return dst_addr;
    }
#endif /* ! STAGE1_5 && ! GRUB_UTIL */
    else
    {
	errnum = ERR_WONT_FIT; return 0;
    }
}
unsigned long long 
grub_memset64(unsigned long long dst_addr, unsigned int data, unsigned long long len)
{
    if (!len)      { errnum=0; return dst_addr; }
    if (!dst_addr) { errnum = ERR_WONT_FIT; return 0; }
    unsigned long highaddr = (unsigned long)( dst_addr       >>32)
			   | (unsigned long)((dst_addr+len-1)>>32);
    if ( highaddr==0 )
    { // below 4GB
	_memset((void*)(unsigned long)dst_addr, data, len);
	errnum = 0; return dst_addr;
    }
#if !defined(STAGE1_5) && !defined(GRUB_UTIL)
    else if ( (highaddr>>(52-32))==0 && (is64bit & IS64BIT_AMD64) && !DISABLE_AMD64)
    { // AMD64/IA32-e paging
	mem64 (3, dst_addr, data, len);	/* 3 for SET */
	return dst_addr;
    }
    else if ( (highaddr>>(52-32))==0 && (is64bit & IS64BIT_PAE))
    { // PAE paging
	void *pdst = DST_VIRTUAL_PTR(dst_addr); 
	unsigned long long dsta = dst_addr;
	memory_paging_enable();
	memory_paging_map_for_transfer(dsta, dsta);
	// transfer
	unsigned long long nr = len; // number of bytes remaining
	while (1)
	{
	    unsigned long n1 = (nr>=PAGINGTXSTEP)? PAGINGTXSTEP: (unsigned long)nr;  // number of bytes per round (8MB)
	    // Copy
	    _memset(pdst, data, n1);
	    // update loop variables
	    if ((nr -= n1)==0) break;
	    dsta+=n1;
	    memory_paging_map_for_transfer(dsta, dsta);
	}
	memory_paging_disable();
	errnum = 0; return dst_addr;
    }
#endif /* ! STAGE1_5 && ! GRUB_UTIL */
    else
    {
	errnum = ERR_WONT_FIT; return 0;
    }
}
int 
grub_memcmp64(unsigned long long str1addr, unsigned long long str2addr, unsigned long long len)
{
    if (!len)      { errnum=0; return 0; }
    unsigned long highaddr = (unsigned long)( str1addr       >>32)
			   | (unsigned long)((str1addr+len-1)>>32)
			   | (unsigned long)( str2addr       >>32)
			   | (unsigned long)((str2addr+len-1)>>32);
    if ( highaddr==0 )
    { // below 4GB
	return _memcmp((const char*)(unsigned long)str1addr,
	    (const char*)(unsigned long)str2addr, (unsigned long)len);
    }
#if !defined(STAGE1_5) && !defined(GRUB_UTIL)
    else if ( (highaddr>>(52-32))==0 && (is64bit & IS64BIT_AMD64) && !DISABLE_AMD64)
    { // AMD64/IA32-e paging
	return mem64 (2, str1addr, str2addr, len);	/* 2 for CMP */
    }
    else if ( (highaddr>>(52-32))==0 && (is64bit & IS64BIT_PAE))
    { // PAE paging
	void *p1 = DST_VIRTUAL_PTR(str1addr); 
	void *p2 = SRC_VIRTUAL_PTR(str2addr); 
	unsigned long long a1=str1addr, a2= str2addr;
	unsigned long long nr = len; // number of bytes remaining
	int r=0;
	memory_paging_enable();
	memory_paging_map_for_transfer(a1, a2);
	{
	    while (1)
	    {
		unsigned long n1 = (nr>=PAGINGTXSTEP)? PAGINGTXSTEP: (unsigned long)nr;  // number of bytes per round (8MB)
		// Compare
		r = _memcmp(p1, p2, n1);
		if (r) break;
		// update loop variables
		if ((nr -= n1)==0) break;
		memory_paging_map_for_transfer((a1+=n1), (a2+=n1));
	    }
	}
	memory_paging_disable();
	return r;
    }
#endif /* ! STAGE1_5 && ! GRUB_UTIL */
    else
    {
	errnum = ERR_WONT_FIT; return 0;
    }
}
