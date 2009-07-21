#define xtoolkit_panel_setup()

/* Show current directory in the xterm title */
/* Show current directory in the xterm title 
   Added possibility to configure xterm title through MC_XTITLE environment variable (like a zsh/bash promt).
   %%    - percent '%' character                                 
   %$,%# - '#' for root, '$' for normal user                    
   %w,%d - working directory with edit/view filename                                   
   %W,%C - working directory (last component only) with edit/view filename
   %p    - process id (pid)
   %u,%n - username
   %h,m  - hostname up to the first dot
   %H,M  - hostname.domainname
   %t,%y - tty connected to stdin without /dev/
   %l    - tty connected to stdin without /dev/ and tty (if present)                             
   %V    - mc version 
*/



GString
get_work_dir ()
{
}

GString
get_username ()
{
}

GString
get_process_id ()
{
}

GString
get_hostname ()
{
}

GString
get_version ()
{
}


void
update_xterm_title_path (void)
{
    int i = 0;
    unsigned char *p, *s;
    unsigned char title [BUF_MEDIUM+1],hostname[MAXHOSTNAMELEN+1],domainname[MAXHOSTNAMELEN+1],nametty[BUF_SMALL+1];

    if (xterm_flag && xterm_title) {
       p = s = g_strdup (strip_home_and_password (current_panel->cwd));
        /* Use special environment variable to format title */
        if ((p = getenv ("MC_XTITLE")) == NULL) {
            p = "mc - %n %m:%w";
        }
 
   do {
            if (*p != '%') {
                title [i++] = *p;
                continue;
            }
            if (!*++p)
                break;
 
            /* Substitute '%' special characters
             * (meaning the same as for bash, but preceded by '%', not '\')
             */
            s = NULL;
            switch (*p) {
            case '%' : /* % - percent '%' character */
                title [i++] = '%';
                break;
            case '#' :
            case '$' : /* %$ or %# - '#' for root, '$' for normal user */
                title [i++] = (geteuid()==0 ? '#' : '$');
                break;
            case 'd' :
            case 'w' : /* %w or %d - working directory */
               if (xterm_filename==NULL) {
                    s = g_strdup (strip_home_and_password (current_panel->cwd));
                } else {
                    if (view_one_file == NULL) {
                        if (g_ascii_strcasecmp (strip_home_and_password (current_panel->cwd),"/")==0) {
                           s = g_strdup_printf("/%s",xterm_filename);
                        } else {
                            s = g_strdup_printf("%s/%s",strip_home_and_password (current_panel->cwd),xterm_filename);
                        }
                    } else {
                        s = g_strdup_printf("%s",strip_home_and_password (xterm_filename));
                    }
                }
                break;
            case 'C' :
            case 'W' : /* %W or %C- working directory (last component only) */
                if (xterm_filename==NULL) {                
                   s = g_strdup (g_path_get_basename ((strip_home_and_password (current_panel->cwd))));
                } else {
                    if (view_one_file == NULL) {                          
                        s = g_strdup_printf ("%s/%s",x_basename (strip_home_and_password (current_panel->cwd)),xterm_filename);
                   } else {
                       s = g_strdup_printf ("%s/%s",x_basename (g_path_get_dirname (strip_home_and_password (xterm_filename))),x_basename(strip_home_and_password(xterm_filename)));
                   }
                }
                break;
            case 'p' : /* %p - process id (pid) */
                s = g_strdup_printf ("%d", getpid ());
                break;
            case 'n' :
            case 'u' : /* %u or %n - username */
                s = g_strdup (getpwuid (getuid ()) -> pw_name);
                break;
            case 'm' :
           case 'h' : /* %h or %m - hostname up to the first dot */
               if (gethostname (hostname,MAXHOSTNAMELEN) == 0) {
                    hostname [strcspn(hostname,".")] = '\0';                    
                    s = g_strdup(hostname);
               }
               break;
            case 'M' :
            case 'H' : /* %H or %M - hostname.domainname */
                if (gethostname (hostname,MAXHOSTNAMELEN) == 0) {
                    hostname [MAXHOSTNAMELEN] = '\0';                
                   if (getdomainname (domainname, MAXHOSTNAMELEN) == 0) {
                       domainname [MAXHOSTNAMELEN] = '\0';                 
                       if (strlen(g_strstrip(hostname))>0 && strlen(g_strstrip(domainname))==0) { 
                           s = g_strdup(hostname);
                       } else if (strlen(g_strstrip(hostname))==0 && strlen(g_strstrip(domainname))>0) {
                           s = g_strdup (domainname);                    
                       } else if (strlen(g_strstrip(hostname))>0 && strlen(g_strstrip(domainname))>0) {
                            if (g_str_has_suffix (g_strstrip(hostname), ".") || g_str_has_prefix (g_strstrip(domainname), ".")) {                              
                               s = g_strconcat(hostname,domainname,NULL);
                           } else {
                                s = g_strconcat(hostname,".",domainname,NULL);
                           }
                       }
                   }
                }
                break;
            case 'y' :
            case 't' : /* %t or %y - tty connected to stdin without /dev/ */                 
                s = g_strdup (x_basename(ttyname (0)));
                break;
            case 'l' : /* %l - tty connected to stdin */
                if (ttyname_r (0,nametty,BUF_SMALL) == 0) {
                    nametty [BUF_SMALL] = '\0';
                   if (g_str_has_prefix (nametty, "/dev/tty")) {
                        strncpy(nametty,nametty+8,BUF_SMALL-8);    
                       s = g_strdup(g_strstrip(nametty));                      
                    } else {
                       s = g_strdup(x_basename(nametty));
                    }
               }
                break;
            case 'V' : /* %V - mc version */
                s = g_strdup (VERSION);
                break;
            }
 
            /* Append substituted string */
            if (s) {
                strncpy (title+i, s, BUF_MEDIUM-i);
                title [BUF_MEDIUM] = '\0';
                i = strlen (title);
                g_free (s);
            }
        } while (*++p && i<BUF_MEDIUM);
         title [i] = '\0';
 
        /* Replace non-printable characters with '?' */
        s = title;
        while (*s) {
       
       if (!is_printable (*s))
       *s = '?';
       } while (*++s);
       fprintf (stdout, "\33]0;mc - %s\7", p);
            s++;
        }
 
        /* Use xterm escape sequence to set window title */
        fprintf (stdout, "\33]0;%s\7", title);
   fflush (stdout);
       g_free (p);
    }
    xterm_filename=NULL; 
}

