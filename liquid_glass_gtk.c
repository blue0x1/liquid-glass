/*
 * Liquid Glass Terminal
 * Author: Chokri Hammedi (blue0x1)
 * https://github.com/blue0x1/liquid-glass
 * License: MIT
 * Copyright (C) 2026 Chokri Hammedi. All rights reserved.
 */

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <vte/vte.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <math.h>
#ifndef M_PI_2
#define M_PI_2 (M_PI/2.0)
#endif
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <ctype.h>
#include "backdrop_refraction.h"

#ifdef EMBED_ICON
#include "liquid_icon_data.h"
static GdkPixbuf* load_icon_pixbuf(int size){
    GdkPixbufLoader*loader=gdk_pixbuf_loader_new();
    gdk_pixbuf_loader_write(loader,(const guchar*)liquid_png,liquid_png_len,NULL);
    gdk_pixbuf_loader_close(loader,NULL);
    GdkPixbuf*src=gdk_pixbuf_loader_get_pixbuf(loader);
    GdkPixbuf*out=NULL;
    if(src){
        out=(size>0)?gdk_pixbuf_scale_simple(src,size,size,GDK_INTERP_BILINEAR):
                     (g_object_ref(src),src);
    }
    g_object_unref(loader);
    return out;
}
#else
static GdkPixbuf* load_icon_pixbuf(int size){
    char path[600];
    snprintf(path,sizeof(path),"%s/.local/share/icons/liquid_glass.png",
        getenv("HOME")?:"/tmp");
    return size>0 ? gdk_pixbuf_new_from_file_at_scale(path,size,size,TRUE,NULL)
                  : gdk_pixbuf_new_from_file(path,NULL);
}
#endif

#define WIN_W      960
#define WIN_H      640
#define SIDEBAR_W  220
#define TAB_H       38
#define MAX_TABS    32
#define MAX_TITLE_LEN 32
#define CFG_PATH   "/.config/liquid_glass/config"

typedef struct {
    double r, g, b, h, s, v;
    double glass_opacity;

    double ct_r, ct_g, ct_b, ct_a;
    double at_r, at_g, at_b, at_a;
    double ab_r, ab_g, ab_b, ab_a;
    double ac_r, ac_g, ac_b, ac_a;
    double ss_a;
} GlassTheme;

static void rgb_to_hsb(double r,double g,double b,double*h,double*s,double*v){
    double mx=fmax(r,fmax(g,b)),mn=fmin(r,fmin(g,b)),d=mx-mn;
    *v=mx; *s=mx>1e-9?d/mx:0;
    if(d<1e-9){*h=0;return;}
    if(mx==r)*h=fmod((g-b)/d,6); else if(mx==g)*h=(b-r)/d+2; else *h=(r-g)/d+4;
    *h/=6; if(*h<0)*h+=1;
}
static void hsb_to_rgb(double h,double s,double v,double*r,double*g,double*b){
    if(s<1e-9){*r=*g=*b=v;return;}
    double hh=fmod(h,1)*6; int i=(int)hh;
    double f=hh-i,p=v*(1-s),q=v*(1-s*f),t=v*(1-s*(1-f));
    switch(i%6){case 0:*r=v;*g=t;*b=p;break;case 1:*r=q;*g=v;*b=p;break;
                case 2:*r=p;*g=v;*b=t;break;case 3:*r=p;*g=q;*b=v;break;
                case 4:*r=t;*g=p;*b=v;break;default:*r=v;*g=p;*b=q;}
}
static int hex_parse(const char*hex,double*r,double*g,double*b){
    const char*s=hex; if(*s=='#')s++;
    if(strlen(s)!=6)return 0;
    unsigned rgb=0; if(sscanf(s,"%x",&rgb)!=1)return 0;
    *r=((rgb>>16)&0xff)/255.0; *g=((rgb>>8)&0xff)/255.0; *b=(rgb&0xff)/255.0; return 1;
}

static void preset_rgb(const char*p,double*r,double*g,double*b){
    if     (!strcmp(p,"original")){*r=0.02;*g=0.05;*b=0.11;}
    else if(!strcmp(p,"ghostty")) {*r=0.16;*g=0.16;*b=0.16;}
    else if(!strcmp(p,"red"))     {*r=0.11;*g=0.02;*b=0.02;}
    else if(!strcmp(p,"blue"))    {*r=0.02;*g=0.02;*b=0.11;}
    else if(!strcmp(p,"yellow"))  {*r=0.11;*g=0.10;*b=0.02;}
    else if(!strcmp(p,"purple"))  {*r=0.06;*g=0.02;*b=0.11;}
    else if(!strcmp(p,"pink"))    {*r=0.13;*g=0.02;*b=0.08;}
    else if(!strcmp(p,"black"))   {*r=0.0; *g=0.0; *b=0.0; }
    else if(!strcmp(p,"gray"))    {*r=0.05;*g=0.05;*b=0.05;}
    else                          {*r=0.02;*g=0.05;*b=0.11;}
}

static void soften_theme_rgb(double*inout_r,double*inout_g,double*inout_b){
    double h,s,v,r,g,b;
    rgb_to_hsb(*inout_r,*inout_g,*inout_b,&h,&s,&v);
    hsb_to_rgb(h,fmin(0.72,s),fmin(0.28,v),&r,&g,&b);
    *inout_r=r; *inout_g=g; *inout_b=b;
}

static void theme_compute(GlassTheme*th){
    double op=fmax(0,fmin(1,th->glass_opacity));

    th->ct_r=th->r; th->ct_g=th->g; th->ct_b=th->b;
    th->ct_a=0.22+op*0.78;
    if(th->s<0.05){

        th->at_r=th->at_g=th->at_b=th->v; th->at_a=0.28+op*0.64;
        th->ab_r=th->ab_g=th->ab_b=th->v; th->ab_a=0.34+op*0.58;
        th->ac_r=th->ac_g=th->ac_b=0;     th->ac_a=0;
    } else {

        hsb_to_rgb(th->h,th->s*0.6,0.84,&th->at_r,&th->at_g,&th->at_b); th->at_a=0.28;
        hsb_to_rgb(th->h,th->s*0.8,0.05,&th->ab_r,&th->ab_g,&th->ab_b); th->ab_a=0.34;
        hsb_to_rgb(th->h,fmax(0.6,th->s),0.8,&th->ac_r,&th->ac_g,&th->ac_b); th->ac_a=0.18;
    }
    th->ss_a=0.072;
}
static void theme_set(GlassTheme*th,const char*preset,const char*hex,double op){
    th->glass_opacity=op;
    if(!strcmp(preset,"custom")) hex_parse(hex,&th->r,&th->g,&th->b);
    else preset_rgb(preset,&th->r,&th->g,&th->b);
    soften_theme_rgb(&th->r,&th->g,&th->b);
    rgb_to_hsb(th->r,th->g,th->b,&th->h,&th->s,&th->v);
    theme_compute(th);
}

typedef struct {
    double glass_opacity;
    char   preset[32];
    char   custom_hex[10];
    int    show_sidebar;
} Config;

static void cfg_default(Config*c){
    c->glass_opacity=0.12; strcpy(c->preset,"original");
    strcpy(c->custom_hex,"#050D1C"); c->show_sidebar=0;
}
static char* cfg_path(void){
    static char buf[512];
    snprintf(buf,sizeof(buf),"%s%s",getenv("HOME")?:"/tmp",CFG_PATH); return buf;
}
static void cfg_load(Config*c){
    cfg_default(c);
    FILE*f=fopen(cfg_path(),"r"); if(!f)return;
    char line[256];
    while(fgets(line,sizeof(line),f)){
        char k[64],v[192];
        if(sscanf(line,"%63[^=]=%191[^\n]",k,v)==2){
            if     (!strcmp(k,"glassOpacity"))  c->glass_opacity=atof(v);
            else if(!strcmp(k,"themePreset"))   snprintf(c->preset,     sizeof(c->preset),    "%s",v);
            else if(!strcmp(k,"themeCustomHex"))snprintf(c->custom_hex, sizeof(c->custom_hex),"%s",v);
            else if(!strcmp(k,"showSidebar"))   c->show_sidebar=0;
        }
    }
    fclose(f);
}
static void cfg_save(const Config*c){
    char*p=cfg_path(); char dir[512]; snprintf(dir,sizeof(dir),"%s",p);
    char*sl=strrchr(dir,'/'); if(sl){*sl=0;mkdir(dir,0755);}
    FILE*f=fopen(p,"w"); if(!f)return;
    fprintf(f,"glassOpacity=%.4f\nthemePreset=%s\nthemeCustomHex=%s\nshowSidebar=%d\n",
        c->glass_opacity,c->preset,c->custom_hex,c->show_sidebar);
    fclose(f);
}

typedef struct { int id; GtkWidget*vte,*tab_btn,*tab_box,*close_btn; char title[128]; } Tab;

typedef struct {
    GtkWidget *window, *headerbar, *root, *hpaned, *sidebar_box, *sidebar_wrap;
    GtkWidget *tab_bar_wrap, *tab_bar, *terminal_overlay, *gl_area, *stack, *max_btn;
    GtkCssProvider *dyn_css;
    Tab        tabs[MAX_TABS];
    int        ntabs, active, next_id, maximized, wm_hover;
    BackdropRefraction refract;
    GlassTheme theme;
    Config     cfg;
    char       exe_path[512];

    GtkWidget *sw_win, *sw_opacity_scale, *sw_opacity_label;
    GtkWidget *sw_preset_combo, *sw_color_btn;
} App;
static App G;

static void sidebar_rebuild(void);
static void open_settings_impl(void);
static void open_settings(GtkWidget*w,gpointer d){(void)w;(void)d;open_settings_impl();}
static void sidebar_toggle_impl(void);
static void sidebar_toggle(GtkWidget*w,gpointer d){(void)w;(void)d;sidebar_toggle_impl();}
static void window_max_toggle(GtkWidget*w,gpointer d);
static gboolean on_window_state(GtkWidget*w,GdkEventWindowState*ev,gpointer d);
static gboolean on_wm_button_draw(GtkWidget*w,cairo_t*cr,gpointer d);
static gboolean on_wm_enter(GtkWidget*w,GdkEventCrossing*e,gpointer d);
static gboolean on_wm_leave(GtkWidget*w,GdkEventCrossing*e,gpointer d);
static gboolean on_tab_clicked(GtkWidget*,gpointer);
static gboolean on_close_clicked(GtkWidget*,gpointer);
static gboolean on_tab_button_press(GtkWidget*,GdkEventButton*,gpointer);
static gboolean on_vte_button_press(GtkWidget*,GdkEventButton*,gpointer);
static void tab_rename(int);

static int add_blur_rect(GtkWidget*child,GtkWidget*win,unsigned long*region,int n){
    if(!child||!gtk_widget_get_visible(child)||!gtk_widget_get_realized(child))return n;
    GtkAllocation a;
    int x=0,y=0;
    gtk_widget_get_allocation(child,&a);
    if(a.width<1||a.height<1)return n;
    if(!gtk_widget_translate_coordinates(child,win,0,0,&x,&y))return n;
    region[n++]=(unsigned long)x;
    region[n++]=(unsigned long)y;
    region[n++]=(unsigned long)a.width;
    region[n++]=(unsigned long)a.height;
    return n;
}

static void apply_blur_hint(GtkWidget*win){
    GdkWindow*gw=gtk_widget_get_window(win);
    if(!gw||!GDK_IS_X11_WINDOW(gw))return;
    Display*dpy=GDK_WINDOW_XDISPLAY(gw);
    Window xw=GDK_WINDOW_XID(gw);

    unsigned long region[20];
    int n=0;
    n=add_blur_rect(G.headerbar,win,region,n);
    n=add_blur_rect(G.root,win,region,n);
    if(n==0){
        n=add_blur_rect(G.tab_bar_wrap,win,region,n);
        n=add_blur_rect(G.sidebar_wrap,win,region,n);
        n=add_blur_rect(G.stack,win,region,n);
    }

    Atom blur_atom=XInternAtom(dpy,"_KDE_NET_WM_BLUR_BEHIND_REGION",False);
    Atom a2=XInternAtom(dpy,"_NET_WM_BLUR_REGION",False);
    if(n>0){
        XChangeProperty(dpy,xw,blur_atom,XA_CARDINAL,32,PropModeReplace,(unsigned char*)region,n);
        XChangeProperty(dpy,xw,a2,XA_CARDINAL,32,PropModeReplace,(unsigned char*)region,n);
    } else {
        XDeleteProperty(dpy,xw,blur_atom);
        XDeleteProperty(dpy,xw,a2);
    }
    XFlush(dpy);
}
static void apply_full_blur_hint(GtkWidget*win){
    GdkWindow*gw=gtk_widget_get_window(win);
    if(!gw||!GDK_IS_X11_WINDOW(gw))return;
    Display*dpy=GDK_WINDOW_XDISPLAY(gw);
    Window xw=GDK_WINDOW_XID(gw);
    GtkAllocation a;
    gtk_widget_get_allocation(win,&a);
    if(a.width<1||a.height<1)return;
    unsigned long region[4]={0,0,(unsigned long)a.width,(unsigned long)a.height};
    Atom blur_atom=XInternAtom(dpy,"_KDE_NET_WM_BLUR_BEHIND_REGION",False);
    Atom a2=XInternAtom(dpy,"_NET_WM_BLUR_REGION",False);
    XChangeProperty(dpy,xw,blur_atom,XA_CARDINAL,32,PropModeReplace,(unsigned char*)region,4);
    XChangeProperty(dpy,xw,a2,XA_CARDINAL,32,PropModeReplace,(unsigned char*)region,4);
    XFlush(dpy);
}

static void on_realize(GtkWidget*w,gpointer d){(void)d;apply_blur_hint(w);}
static void on_settings_realize(GtkWidget*w,gpointer d){
    (void)d;
    GdkWindow*gw=gtk_widget_get_window(w);
    if(!gw||!GDK_IS_X11_WINDOW(gw))return;
    Display*dpy=GDK_WINDOW_XDISPLAY(gw);
    Window xw=GDK_WINDOW_XID(gw);
    Atom blur_atom=XInternAtom(dpy,"_KDE_NET_WM_BLUR_BEHIND_REGION",False);
    Atom a2=XInternAtom(dpy,"_NET_WM_BLUR_REGION",False);
    XDeleteProperty(dpy,xw,blur_atom);
    XDeleteProperty(dpy,xw,a2);
    XFlush(dpy);
}
static gboolean on_clear_draw(GtkWidget*w,cairo_t*cr,gpointer d){
    (void)w;(void)d;
    cairo_set_operator(cr,CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr,CAIRO_OPERATOR_OVER);
    return FALSE;
}
static void on_layout_changed(GtkWidget*w,GtkAllocation*a,gpointer d){
    (void)w;(void)a;(void)d;
    if(G.window&&gtk_widget_get_realized(G.window)) apply_blur_hint(G.window);
}

static gboolean sidebar_show_idle(gpointer d){
    (void)d;
    if(G.hpaned){
        if(G.cfg.show_sidebar)
            gtk_paned_set_position(GTK_PANED(G.hpaned),SIDEBAR_W);
        else
            gtk_paned_set_position(GTK_PANED(G.hpaned),0);
    }
    if(G.window&&gtk_widget_get_realized(G.window)) apply_blur_hint(G.window);
    return G_SOURCE_REMOVE;
}

static void max_button_sync(void){
    (void)G.max_btn;
}

static void window_max_toggle(GtkWidget*w,gpointer d){
    (void)w;(void)d;
    if(G.maximized) gtk_window_unmaximize(GTK_WINDOW(G.window));
    else gtk_window_maximize(GTK_WINDOW(G.window));
}

static gboolean on_window_state(GtkWidget*w,GdkEventWindowState*ev,gpointer d){
    (void)w;(void)d;
    if(ev->changed_mask&GDK_WINDOW_STATE_MAXIMIZED){
        G.maximized=(ev->new_window_state&GDK_WINDOW_STATE_MAXIMIZED)!=0;
        max_button_sync();
        if(G.max_btn) gtk_widget_queue_draw(G.max_btn);
    }
    return FALSE;
}

static gboolean on_wm_button_draw(GtkWidget*w,cairo_t*cr,gpointer d){
    int kind=GPOINTER_TO_INT(d);
    if(G.wm_hover!=kind+1)return FALSE;
    GtkAllocation a;
    gtk_widget_get_allocation(w,&a);
    double cx=a.width/2.0, cy=a.height/2.0;

    cairo_set_source_rgba(cr,0,0,0,0.58);
    cairo_set_line_width(cr,1.35);
    cairo_set_line_cap(cr,CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr,CAIRO_LINE_JOIN_ROUND);

    if(kind==0){
        cairo_move_to(cr,cx-3,cy-3); cairo_line_to(cr,cx+3,cy+3);
        cairo_move_to(cr,cx+3,cy-3); cairo_line_to(cr,cx-3,cy+3);
    } else if(kind==1){
        cairo_move_to(cr,cx-3.5,cy); cairo_line_to(cr,cx+3.5,cy);
    } else {
        if(G.maximized){
            cairo_move_to(cr,cx-4,cy-4); cairo_line_to(cr,cx-1,cy-1);
            cairo_move_to(cr,cx-4,cy-1.8); cairo_line_to(cr,cx-4,cy-4); cairo_line_to(cr,cx-1.8,cy-4);
            cairo_move_to(cr,cx+4,cy+4); cairo_line_to(cr,cx+1,cy+1);
            cairo_move_to(cr,cx+4,cy+1.8); cairo_line_to(cr,cx+4,cy+4); cairo_line_to(cr,cx+1.8,cy+4);
        } else {
            cairo_set_line_width(cr,1.55);
            cairo_move_to(cr,cx+0.9,cy-0.9); cairo_line_to(cr,cx+3.5,cy-3.5);
            cairo_move_to(cr,cx+3.5,cy-3.5); cairo_line_to(cr,cx+3.5,cy-1.3);
            cairo_move_to(cr,cx+3.5,cy-3.5); cairo_line_to(cr,cx+1.3,cy-3.5);

            cairo_move_to(cr,cx-0.9,cy+0.9); cairo_line_to(cr,cx-3.5,cy+3.5);
            cairo_move_to(cr,cx-3.5,cy+3.5); cairo_line_to(cr,cx-3.5,cy+1.3);
            cairo_move_to(cr,cx-3.5,cy+3.5); cairo_line_to(cr,cx-1.3,cy+3.5);
        }
    }
    cairo_stroke(cr);
    return FALSE;
}

static gboolean on_wm_enter(GtkWidget*w,GdkEventCrossing*e,gpointer d){
    (void)e;
    G.wm_hover=GPOINTER_TO_INT(d)+1;
    gtk_widget_queue_draw(w);
    return FALSE;
}

static gboolean on_wm_leave(GtkWidget*w,GdkEventCrossing*e,gpointer d){
    (void)e;(void)d;
    G.wm_hover=0;
    gtk_widget_queue_draw(w);
    return FALSE;
}

static void dyn_css_update(void){
    const GlassTheme*th=&G.theme;
    char buf[2048];

    int custom = !strcmp(G.cfg.preset,"custom");
    double panel_a = custom ? fmin(0.58,th->ct_a * 0.46) : fmin(0.78,th->ct_a * 0.68);
    double base_a = custom ? fmin(0.22,th->ct_a * 0.16) : fmin(0.34,th->ct_a * 0.26);
    double header_a = custom ? fmin(0.52,th->ct_a * 0.40) : fmin(0.70,th->ct_a * 0.60);
    snprintf(buf,sizeof(buf),
        "#root { background: rgba(%d,%d,%d,%.3f); }"
        "headerbar { background: rgba(%d,%d,%d,%.3f); border-bottom: 1px solid rgba(255,255,255,%.3f); }"
        "#tab-bar { background: rgba(%d,%d,%d,%.3f); }"
        "#tab-bar-bottom { background: rgba(255,255,255,%.3f); }"
        "#sidebar { background: rgba(%d,%d,%d,%.3f); }"
        "#sidebar-divider { background: rgba(255,255,255,%.3f); }"
        "#settings-window { background: rgba(%d,%d,%d,%.3f); }"
        "#settings-header { background: rgba(%d,%d,%d,%.3f); border-bottom: 1px solid rgba(255,255,255,%.3f); }"
        "#settings-root { background: rgba(%d,%d,%d,%.3f); }"
        "#about-root { background: rgba(%d,%d,%d,1.00); }",
        (int)(th->ct_r*255),(int)(th->ct_g*255),(int)(th->ct_b*255), base_a,
        (int)(th->ct_r*255),(int)(th->ct_g*255),(int)(th->ct_b*255), header_a, th->ss_a*0.20,
        (int)(th->ct_r*255),(int)(th->ct_g*255),(int)(th->ct_b*255), panel_a,
        th->ss_a*0.20,
        (int)(th->ct_r*255),(int)(th->ct_g*255),(int)(th->ct_b*255), panel_a,
        th->ss_a*0.40,
        (int)(th->ct_r*255),(int)(th->ct_g*255),(int)(th->ct_b*255), fmin(0.90,th->ct_a*0.72),
        (int)(th->ct_r*255),(int)(th->ct_g*255),(int)(th->ct_b*255), header_a, th->ss_a*0.20,
        (int)(th->ct_r*255),(int)(th->ct_g*255),(int)(th->ct_b*255), fmin(0.82,th->ct_a*0.58),
        (int)(th->ct_r*255),(int)(th->ct_g*255),(int)(th->ct_b*255)
    );
    gtk_css_provider_load_from_data(G.dyn_css, buf, -1, NULL);
}

static void current_theme_color(GdkRGBA*c){
    if(!strcmp(G.cfg.preset,"custom")){
        hex_parse(G.cfg.custom_hex,&c->red,&c->green,&c->blue);
    } else {
        preset_rgb(G.cfg.preset,&c->red,&c->green,&c->blue);
    }
    soften_theme_rgb(&c->red,&c->green,&c->blue);
    c->alpha=1.0;
}

static void rrect(cairo_t*cr,double x,double y,double w,double h,double r){
    cairo_move_to(cr,x+r,y);
    cairo_line_to(cr,x+w-r,y);
    cairo_arc(cr,x+w-r,y+r,r,-M_PI_2,0);
    cairo_line_to(cr,x+w,y+h-r);
    cairo_arc(cr,x+w-r,y+h-r,r,0,M_PI_2);
    cairo_line_to(cr,x+r,y+h);
    cairo_arc(cr,x+r,y+h-r,r,M_PI_2,M_PI);
    cairo_line_to(cr,x,y+r);
    cairo_arc(cr,x+r,y+r,r,M_PI,3*M_PI_2);
    cairo_close_path(cr);
}

#define CORNER_R 12.0

static gboolean window_draw(GtkWidget*w,cairo_t*cr,gpointer d){
    (void)w;(void)d;

    cairo_set_operator(cr,CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr,CAIRO_OPERATOR_OVER);
    return FALSE;
}

static void gl_area_realize(GtkGLArea*area,gpointer d){
    (void)d;
    backdrop_refraction_realize(&G.refract,area);
}

static void gl_area_unrealize(GtkGLArea*area,gpointer d){
    (void)d;
    backdrop_refraction_unrealize(&G.refract,area);
}

static gboolean gl_area_render(GtkGLArea*area,GdkGLContext*context,gpointer d){
    (void)context;(void)d;
    double op=fmax(0.0,fmin(1.0,G.cfg.glass_opacity));
    return backdrop_refraction_render(&G.refract,area,G.window,GTK_WIDGET(area),
        G.theme.ct_r,G.theme.ct_g,G.theme.ct_b,G.theme.ct_a,op);
}

static gboolean gl_tick(GtkWidget*w,GdkFrameClock*clock,gpointer d){
    (void)clock;(void)d;
    gtk_widget_queue_draw(w);
    return G_SOURCE_CONTINUE;
}

static void vte_setup(GtkWidget*vte){

    GdkScreen*sc=gdk_screen_get_default();
    GdkVisual*rgba=gdk_screen_get_rgba_visual(sc);
    if(rgba) gtk_widget_set_visual(vte,rgba);

}
static void vte_colors(GtkWidget*vte){
    const GlassTheme*th=&G.theme;

    double tint_a = !strcmp(G.cfg.preset,"custom") ? fmin(0.13,th->ct_a*0.10) : fmin(0.18,th->ct_a*0.13);
    GdkRGBA bg={th->ct_r,th->ct_g,th->ct_b,tint_a};
    GdkRGBA fg={0.92,0.93,0.97,1};
    GdkRGBA cur={1,1,1,0.9};
    vte_terminal_set_color_background(VTE_TERMINAL(vte),&bg);
    vte_terminal_set_color_foreground(VTE_TERMINAL(vte),&fg);
    vte_terminal_set_color_bold(VTE_TERMINAL(vte),&fg);
    vte_terminal_set_color_cursor(VTE_TERMINAL(vte),&cur);
    vte_terminal_set_color_cursor_foreground(VTE_TERMINAL(vte),NULL);
    GdkRGBA pal[16]={
        {0.07,0.09,0.14,1},{0.92,0.28,0.28,1},{0.30,0.88,0.44,1},{0.96,0.84,0.20,1},
        {0.40,0.64,1.00,1},{0.80,0.36,0.96,1},{0.28,0.88,0.96,1},{0.90,0.90,0.94,1},
        {0.26,0.30,0.42,1},{1.00,0.46,0.46,1},{0.46,1.00,0.54,1},{1.00,0.96,0.40,1},
        {0.56,0.80,1.00,1},{0.96,0.60,1.00,1},{0.50,0.96,1.00,1},{1.00,1.00,1.00,1},
    };
    vte_terminal_set_colors(VTE_TERMINAL(vte),&fg,&bg,pal,16);
    PangoFontDescription*fd=pango_font_description_from_string("Monospace 11");
    vte_terminal_set_font(VTE_TERMINAL(vte),fd);
    pango_font_description_free(fd);
    vte_terminal_set_scrollback_lines(VTE_TERMINAL(vte),10000);
    vte_terminal_set_audible_bell(VTE_TERMINAL(vte),FALSE);
    vte_terminal_set_scroll_on_keystroke(VTE_TERMINAL(vte),TRUE);
    vte_terminal_set_mouse_autohide(VTE_TERMINAL(vte),TRUE);
}
static void vte_colors_all(void){
    for(int i=0;i<G.ntabs;i++) vte_colors(G.tabs[i].vte);
}

static void theme_apply(void){
    theme_set(&G.theme,G.cfg.preset,G.cfg.custom_hex,G.cfg.glass_opacity);
    dyn_css_update();
    vte_colors_all();
    if(G.window) gtk_widget_queue_draw(G.window);
    if(G.gl_area) gtk_widget_queue_draw(G.gl_area);
    if(G.window&&gtk_widget_get_realized(G.window)) apply_blur_hint(G.window);
}

static void tab_style_update(void){
    for(int i=0;i<G.ntabs;i++){
        GtkStyleContext*ctx=gtk_widget_get_style_context(G.tabs[i].tab_btn);
        if(i==G.active){gtk_style_context_add_class(ctx,"tab-active");gtk_style_context_remove_class(ctx,"tab-inactive");}
        else{gtk_style_context_remove_class(ctx,"tab-inactive");gtk_style_context_add_class(ctx,"tab-inactive");}
    }
}
static void tab_switch(int idx);

static void tab_rebind_handlers(void){
    for(int i=0;i<G.ntabs;i++){
        g_signal_handlers_disconnect_matched(G.tabs[i].tab_btn,
            G_SIGNAL_MATCH_FUNC,0,0,NULL,G_CALLBACK(on_tab_clicked),NULL);
        g_signal_handlers_disconnect_matched(G.tabs[i].tab_btn,
            G_SIGNAL_MATCH_FUNC,0,0,NULL,G_CALLBACK(on_tab_button_press),NULL);
        g_signal_handlers_disconnect_matched(G.tabs[i].close_btn,
            G_SIGNAL_MATCH_FUNC,0,0,NULL,G_CALLBACK(on_close_clicked),NULL);
        g_signal_connect(G.tabs[i].tab_btn,  "clicked",G_CALLBACK(on_tab_clicked),  GINT_TO_POINTER(i));
        g_signal_connect(G.tabs[i].tab_btn,  "button-press-event",G_CALLBACK(on_tab_button_press),GINT_TO_POINTER(i));
        g_signal_connect(G.tabs[i].close_btn,"clicked",G_CALLBACK(on_close_clicked),GINT_TO_POINTER(i));
    }
}

static int tab_add(void){
    if(G.ntabs>=MAX_TABS)return -1;
    int idx=G.ntabs++; Tab*t=&G.tabs[idx];
    t->id=G.next_id++;
    snprintf(t->title,sizeof(t->title),"Terminal %d",t->id+1);

    t->vte=vte_terminal_new();
    vte_setup(t->vte);
    gtk_widget_set_hexpand(t->vte,TRUE); gtk_widget_set_vexpand(t->vte,TRUE);
    vte_colors(t->vte);
    g_signal_connect(t->vte,"button-press-event",G_CALLBACK(on_vte_button_press),NULL);

    const char*sh=getenv("SHELL"); if(!sh||!sh[0])sh="/bin/bash";
    char*args[]={(char*)sh,NULL};
    vte_terminal_spawn_async(VTE_TERMINAL(t->vte),VTE_PTY_DEFAULT,
        NULL,args,NULL,(GSpawnFlags)0,NULL,NULL,NULL,-1,NULL,NULL,NULL);

    char name[32]; snprintf(name,sizeof(name),"tab%d",t->id);
    gtk_stack_add_named(GTK_STACK(G.stack),t->vte,name);
    gtk_widget_show(t->vte);

    t->tab_box=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    t->tab_btn=gtk_button_new_with_label(t->title);
    t->close_btn=gtk_button_new_with_label("×");
    gtk_widget_set_name(t->tab_btn,"tab-btn");
    gtk_widget_set_name(t->close_btn,"tab-close");
    gtk_widget_set_size_request(t->close_btn,12,12);
    g_signal_connect(t->tab_btn,  "clicked",G_CALLBACK(on_tab_clicked),  GINT_TO_POINTER(idx));
    g_signal_connect(t->tab_btn,  "button-press-event",G_CALLBACK(on_tab_button_press),GINT_TO_POINTER(idx));
    g_signal_connect(t->close_btn,"clicked",G_CALLBACK(on_close_clicked),GINT_TO_POINTER(idx));
    gtk_box_pack_start(GTK_BOX(t->tab_box),t->tab_btn,  FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(t->tab_box),t->close_btn,FALSE,FALSE,0);

    GList*ch=gtk_container_get_children(GTK_CONTAINER(G.tab_bar));
    int pos=(int)g_list_length(ch)-1; if(pos<0)pos=0; g_list_free(ch);
    gtk_box_pack_start(GTK_BOX(G.tab_bar),t->tab_box,FALSE,FALSE,0);
    gtk_box_reorder_child(GTK_BOX(G.tab_bar),t->tab_box,pos);
    gtk_widget_show_all(t->tab_box);

    sidebar_rebuild();
    return idx;
}

static void tab_switch(int idx){
    if(idx<0||idx>=G.ntabs)return;
    G.active=idx;
    char name[32]; snprintf(name,sizeof(name),"tab%d",G.tabs[idx].id);
    gtk_stack_set_visible_child_name(GTK_STACK(G.stack),name);
    gtk_widget_grab_focus(G.tabs[idx].vte);
    tab_style_update(); sidebar_rebuild();
}

static void tab_close(int idx){
    if(G.ntabs<=1){gtk_main_quit();return;}
    Tab*t=&G.tabs[idx];
    gtk_container_remove(GTK_CONTAINER(G.stack),  t->vte);
    gtk_container_remove(GTK_CONTAINER(G.tab_bar),t->tab_box);
    for(int i=idx;i<G.ntabs-1;i++) G.tabs[i]=G.tabs[i+1];
    G.ntabs--;
    tab_rebind_handlers();
    tab_switch(idx<G.ntabs?idx:G.ntabs-1);
}

static void tab_move(int idx,int dir){
    int dst=idx+dir;
    if(idx<0||idx>=G.ntabs||dst<0||dst>=G.ntabs)return;
    Tab tmp=G.tabs[idx];
    G.tabs[idx]=G.tabs[dst];
    G.tabs[dst]=tmp;
    if(G.active==idx)G.active=dst;
    else if(G.active==dst)G.active=idx;
    gtk_box_reorder_child(GTK_BOX(G.tab_bar),G.tabs[idx].tab_box,idx);
    gtk_box_reorder_child(GTK_BOX(G.tab_bar),G.tabs[dst].tab_box,dst);
    tab_rebind_handlers();
    tab_switch(G.active);
}

static int sanitize_tab_title(const char*in,char*out,size_t out_sz){
    if(!in||!out||out_sz<2)return 0;
    while(*in&&isspace((unsigned char)*in))in++;

    size_t n=0;
    for(;*in&&n<out_sz-1&&n<MAX_TITLE_LEN;in++){
        unsigned char c=(unsigned char)*in;
        if(isalnum(c)||c==' '||c=='.'||c=='-'||c=='_')
            out[n++]=(char)c;
        else
            return 0;
    }
    while(n>0&&isspace((unsigned char)out[n-1]))n--;
    out[n]=0;
    return n>0;
}

static int tab_index_from_vte(GtkWidget*vte){
    for(int i=0;i<G.ntabs;i++)
        if(G.tabs[i].vte==vte)return i;
    return -1;
}

static gboolean on_tab_clicked  (GtkWidget*w,gpointer d){(void)w;tab_switch(GPOINTER_TO_INT(d));return FALSE;}
static gboolean on_close_clicked(GtkWidget*w,gpointer d){(void)w;tab_close(GPOINTER_TO_INT(d));return FALSE;}
static gboolean on_add_tab      (GtkWidget*w,gpointer d){(void)w;(void)d;tab_switch(tab_add());return FALSE;}

static void on_menu_set_title(GtkWidget*w,gpointer d){(void)w;tab_rename(GPOINTER_TO_INT(d));}
static void on_menu_move_left(GtkWidget*w,gpointer d){(void)w;tab_move(GPOINTER_TO_INT(d),-1);}
static void on_menu_move_right(GtkWidget*w,gpointer d){(void)w;tab_move(GPOINTER_TO_INT(d),1);}
static void on_menu_close_tab(GtkWidget*w,gpointer d){(void)w;tab_close(GPOINTER_TO_INT(d));}

static void on_vte_menu_copy(GtkWidget*w,gpointer d){
    (void)w;
    int idx=GPOINTER_TO_INT(d);
    if(idx>=0&&idx<G.ntabs)
        vte_terminal_copy_clipboard_format(VTE_TERMINAL(G.tabs[idx].vte),VTE_FORMAT_TEXT);
}
static void on_vte_menu_paste(GtkWidget*w,gpointer d){
    (void)w;
    int idx=GPOINTER_TO_INT(d);
    if(idx>=0&&idx<G.ntabs)
        vte_terminal_paste_clipboard(VTE_TERMINAL(G.tabs[idx].vte));
}
static void on_vte_menu_open_terminal(GtkWidget*w,gpointer d){
    (void)w;(void)d;
    char*args[]={(char*)(G.exe_path[0]?G.exe_path:"./liquid_glass_gtk"),NULL};
    g_spawn_async(NULL,args,NULL,G_SPAWN_SEARCH_PATH,NULL,NULL,NULL,NULL);
}
static void on_vte_menu_open_tab(GtkWidget*w,gpointer d){
    (void)w;(void)d;
    tab_switch(tab_add());
}

static void tab_rename(int idx){
    if(idx<0||idx>=G.ntabs)return;

    GtkWidget*dlg=gtk_dialog_new_with_buttons("Rename Tab",
        GTK_WINDOW(G.window),
        GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Cancel",GTK_RESPONSE_CANCEL,
        "_Rename",GTK_RESPONSE_OK,
        NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg),GTK_RESPONSE_OK);

    GtkWidget*area=gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    GtkWidget*box=gtk_box_new(GTK_ORIENTATION_VERTICAL,8);
    gtk_widget_set_margin_start(box,16);
    gtk_widget_set_margin_end(box,16);
    gtk_widget_set_margin_top(box,14);
    gtk_widget_set_margin_bottom(box,10);

    GtkWidget*entry=gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(entry),MAX_TITLE_LEN);
    gtk_entry_set_text(GTK_ENTRY(entry),G.tabs[idx].title);
    gtk_entry_set_activates_default(GTK_ENTRY(entry),TRUE);
    gtk_editable_select_region(GTK_EDITABLE(entry),0,-1);

    gtk_box_pack_start(GTK_BOX(box),entry,FALSE,FALSE,0);
    gtk_container_add(GTK_CONTAINER(area),box);
    gtk_widget_show_all(dlg);

    if(gtk_dialog_run(GTK_DIALOG(dlg))==GTK_RESPONSE_OK){
        const char*txt=gtk_entry_get_text(GTK_ENTRY(entry));
        char safe[sizeof(G.tabs[idx].title)];
        if(sanitize_tab_title(txt,safe,sizeof(safe))){
            snprintf(G.tabs[idx].title,sizeof(G.tabs[idx].title),"%s",safe);
            gtk_button_set_label(GTK_BUTTON(G.tabs[idx].tab_btn),G.tabs[idx].title);
            sidebar_rebuild();
        } else {
            GtkWidget*err=gtk_message_dialog_new(GTK_WINDOW(G.window),
                GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_ERROR,
                GTK_BUTTONS_OK,
                "Tab title can only use letters, numbers, spaces, dots, dashes, and underscores.");
            gtk_dialog_run(GTK_DIALOG(err));
            gtk_widget_destroy(err);
        }
    }
    gtk_widget_destroy(dlg);
}

static gboolean on_tab_button_press(GtkWidget*w,GdkEventButton*ev,gpointer d){
    if(ev->type==GDK_BUTTON_PRESS&&ev->button==3){
        int idx=GPOINTER_TO_INT(d);
        GtkWidget*menu=gtk_menu_new();
        gtk_style_context_add_class(gtk_widget_get_style_context(menu),"context-menu");
        GtkWidget*set_title=gtk_menu_item_new_with_label("Set Title");
        g_signal_connect(set_title,"activate",G_CALLBACK(on_menu_set_title),GINT_TO_POINTER(idx));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu),set_title);

        if(idx>0){
            GtkWidget*move_left=gtk_menu_item_new_with_label("Move Left");
            g_signal_connect(move_left,"activate",G_CALLBACK(on_menu_move_left),GINT_TO_POINTER(idx));
            gtk_menu_shell_append(GTK_MENU_SHELL(menu),move_left);
        }
        if(idx<G.ntabs-1){
            GtkWidget*move_right=gtk_menu_item_new_with_label("Move Right");
            g_signal_connect(move_right,"activate",G_CALLBACK(on_menu_move_right),GINT_TO_POINTER(idx));
            gtk_menu_shell_append(GTK_MENU_SHELL(menu),move_right);
        }

        GtkWidget*sep=gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(menu),sep);

        GtkWidget*close=gtk_menu_item_new_with_label("Close Tab");
        g_signal_connect(close,"activate",G_CALLBACK(on_menu_close_tab),GINT_TO_POINTER(idx));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu),close);

        gtk_widget_show_all(menu);
        gtk_menu_popup_at_pointer(GTK_MENU(menu),(GdkEvent*)ev);
        return TRUE;
    }
    (void)w;
    return FALSE;
}

static gboolean on_vte_button_press(GtkWidget*w,GdkEventButton*ev,gpointer d){
    (void)d;
    if(ev->type!=GDK_BUTTON_PRESS||ev->button!=3)return FALSE;

    int idx=tab_index_from_vte(w);
    if(idx<0)return FALSE;

    GtkWidget*menu=gtk_menu_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(menu),"context-menu");

    if(vte_terminal_get_has_selection(VTE_TERMINAL(w))){
        GtkWidget*copy=gtk_menu_item_new_with_label("Copy");
        g_signal_connect(copy,"activate",G_CALLBACK(on_vte_menu_copy),GINT_TO_POINTER(idx));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu),copy);
    }

    GtkClipboard*clip=gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    if(gtk_clipboard_wait_is_text_available(clip)){
        GtkWidget*paste=gtk_menu_item_new_with_label("Paste");
        g_signal_connect(paste,"activate",G_CALLBACK(on_vte_menu_paste),GINT_TO_POINTER(idx));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu),paste);
    }

    GtkWidget*open_terminal=gtk_menu_item_new_with_label("Open Terminal");
    g_signal_connect(open_terminal,"activate",G_CALLBACK(on_vte_menu_open_terminal),NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),open_terminal);

    GtkWidget*open_tab=gtk_menu_item_new_with_label("Open Tab");
    g_signal_connect(open_tab,"activate",G_CALLBACK(on_vte_menu_open_tab),NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),open_tab);

    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu),(GdkEvent*)ev);
    return TRUE;
}

static gboolean on_sidebar_tab(GtkWidget*w,gpointer d){(void)w;tab_switch(GPOINTER_TO_INT(d));return FALSE;}

static void sidebar_rebuild(void){
    if(!G.sidebar_box)return;
    GList*ch=gtk_container_get_children(GTK_CONTAINER(G.sidebar_box));
    int i=0;
    for(GList*l=ch;l;l=l->next,i++) if(i>0) gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(ch);

    GtkWidget*ghdr=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6);
    gtk_widget_set_name(ghdr,"sb-group-hdr");
    GtkWidget*garr=gtk_label_new("▾");
    GtkWidget*glbl=gtk_label_new("Group 1");
    gtk_widget_set_name(glbl,"sb-group-lbl");
    GtkWidget*gcnt=gtk_label_new(NULL);
    {char s[8]; snprintf(s,sizeof(s),"%d",G.ntabs); gtk_label_set_text(GTK_LABEL(gcnt),s);}
    gtk_widget_set_name(gcnt,"sb-group-cnt");
    gtk_box_pack_start(GTK_BOX(ghdr),garr,FALSE,FALSE,14);
    gtk_box_pack_start(GTK_BOX(ghdr),glbl,FALSE,FALSE,0);
    gtk_box_pack_end  (GTK_BOX(ghdr),gcnt,FALSE,FALSE,14);
    gtk_box_pack_start(GTK_BOX(G.sidebar_box),ghdr,FALSE,FALSE,0);

    for(int j=0;j<G.ntabs;j++){
        GtkWidget*row=gtk_button_new();
        GtkWidget*box=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,4);
        GtkWidget*icon=gtk_label_new("⌨");
        GtkWidget*lbl=gtk_label_new(G.tabs[j].title);
        gtk_label_set_xalign(GTK_LABEL(lbl),0);
        gtk_widget_set_name(icon,"sb-tab-icon");
        gtk_widget_set_name(lbl, j==G.active?"sb-tab-active":"sb-tab");
        gtk_box_pack_start(GTK_BOX(box),icon,FALSE,FALSE,14);
        gtk_box_pack_start(GTK_BOX(box),lbl, TRUE, TRUE, 0);
        gtk_container_add(GTK_CONTAINER(row),box);
        gtk_widget_set_name(row, j==G.active?"sb-tab-row-active":"sb-tab-row");
        g_signal_connect(row,"clicked",G_CALLBACK(on_sidebar_tab),GINT_TO_POINTER(j));
        gtk_box_pack_start(GTK_BOX(G.sidebar_box),row,FALSE,FALSE,0);
    }

    GtkWidget*div=gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_name(div,"sb-divider");
    gtk_box_pack_end(GTK_BOX(G.sidebar_box),div,FALSE,FALSE,0);

    gtk_widget_show_all(G.sidebar_box);
}

static void sidebar_toggle_impl(void){
    G.cfg.show_sidebar=!G.cfg.show_sidebar;
    if(G.cfg.show_sidebar){
        gtk_widget_set_no_show_all(G.sidebar_wrap,FALSE);
        gtk_widget_show(G.sidebar_wrap);
        gtk_paned_set_position(GTK_PANED(G.hpaned),SIDEBAR_W);
    } else {
        gtk_widget_set_no_show_all(G.sidebar_wrap,TRUE);
        gtk_widget_hide(G.sidebar_wrap);
        gtk_paned_set_position(GTK_PANED(G.hpaned),0);
    }
}

static const struct{const char*id,*label;}PRESETS[]={
    {"original","Blue Frost"},{"ghostty","Graphite"},{"red","Red"},{"blue","Blue"},
    {"yellow","Yellow"},{"purple","Purple"},{"pink","Pink"},{"black","Black"},{"gray","Gray"},
    {"custom","Custom"},{NULL,NULL}
};

static void sw_opacity_changed(GtkRange*r,gpointer d){
    (void)d;
    G.cfg.glass_opacity=gtk_range_get_value(r);
    if(G.sw_opacity_label){
        char s[16]; snprintf(s,sizeof(s),"%.2f",G.cfg.glass_opacity);
        gtk_label_set_text(GTK_LABEL(G.sw_opacity_label),s);
    }
    theme_apply();
}
static void sw_preset_changed(GtkComboBox*cb,gpointer d){
    (void)d;
    int i=gtk_combo_box_get_active(cb); if(i<0||!PRESETS[i].id)return;
    snprintf(G.cfg.preset,sizeof(G.cfg.preset),"%s",PRESETS[i].id);
    theme_apply();
    if(G.sw_color_btn){
        GdkRGBA c;
        current_theme_color(&c);
        gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(G.sw_color_btn),&c);
    }
}
static void sw_color_changed(GtkColorButton*b,gpointer d){
    (void)d;
    GdkRGBA c;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(b),&c);
    double r=c.red,g=c.green,bb=c.blue;
    soften_theme_rgb(&r,&g,&bb);
    snprintf(G.cfg.custom_hex,sizeof(G.cfg.custom_hex),"#%02X%02X%02X",
        (int)round(fmax(0,fmin(1,r))*255),
        (int)round(fmax(0,fmin(1,g))*255),
        (int)round(fmax(0,fmin(1,bb))*255));
    c.red=r; c.green=g; c.blue=bb; c.alpha=1.0;
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(b),&c);
    snprintf(G.cfg.preset,sizeof(G.cfg.preset),"custom");
    if(G.sw_preset_combo){
        for(int i=0;PRESETS[i].id;i++)
            if(!strcmp(PRESETS[i].id,"custom")){gtk_combo_box_set_active(GTK_COMBO_BOX(G.sw_preset_combo),i);break;}
    }
    theme_apply();
}
static void sw_save(GtkButton*b,gpointer d){(void)b;(void)d; cfg_save(&G.cfg); gtk_widget_hide(G.sw_win);}
static void sw_close(GtkButton*b,gpointer d){(void)b;(void)d; if(G.sw_win) gtk_widget_destroy(G.sw_win);}

static void open_about_impl(void){
    GtkWidget*win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(win,"settings-window");
    GdkScreen*screen=gdk_screen_get_default();
    GdkVisual*rgba_vis=gdk_screen_get_rgba_visual(screen);
    if(rgba_vis){ gtk_widget_set_visual(win,rgba_vis); gtk_widget_set_app_paintable(win,TRUE); }
    gtk_window_set_default_size(GTK_WINDOW(win),380,260);
    gtk_window_set_resizable(GTK_WINDOW(win),FALSE);
    gtk_window_set_transient_for(GTK_WINDOW(win),GTK_WINDOW(G.window));
    gtk_window_set_position(GTK_WINDOW(win),GTK_WIN_POS_CENTER_ON_PARENT);
    g_signal_connect(win,"realize",G_CALLBACK(on_settings_realize),NULL);
    g_signal_connect(win,"draw",G_CALLBACK(on_clear_draw),NULL);

    GtkWidget*hbar=gtk_header_bar_new();
    gtk_widget_set_name(hbar,"settings-header");
    gtk_header_bar_set_title(GTK_HEADER_BAR(hbar),"About");
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(hbar),FALSE);
    GtkWidget*close_btn=gtk_button_new_with_label("×");
    gtk_widget_set_name(close_btn,"tab-close");
    gtk_widget_set_size_request(close_btn,12,12);
    gtk_button_set_relief(GTK_BUTTON(close_btn),GTK_RELIEF_NONE);
    g_signal_connect_swapped(close_btn,"clicked",G_CALLBACK(gtk_widget_destroy),win);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(hbar),close_btn);
    gtk_window_set_titlebar(GTK_WINDOW(win),hbar);

    GtkWidget*vb=gtk_box_new(GTK_ORIENTATION_VERTICAL,14);
    gtk_widget_set_name(vb,"about-root");
    gtk_widget_set_hexpand(vb,TRUE);
    gtk_widget_set_vexpand(vb,TRUE);
    gtk_container_add(GTK_CONTAINER(win),vb);

    GtkWidget*spacer_top=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
    gtk_widget_set_vexpand(spacer_top,TRUE);
    gtk_box_pack_start(GTK_BOX(vb),spacer_top,TRUE,TRUE,0);

    GdkPixbuf*pb=load_icon_pixbuf(72);
    if(pb){
        GtkWidget*img=gtk_image_new_from_pixbuf(pb);
        gtk_widget_set_halign(img,GTK_ALIGN_CENTER);
        gtk_box_pack_start(GTK_BOX(vb),img,FALSE,FALSE,0);
        g_object_unref(pb);
    }

    GtkWidget*name_lbl=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(name_lbl),"<span size='x-large' weight='bold'>Liquid Glass</span>");
    gtk_widget_set_halign(name_lbl,GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(vb),name_lbl,FALSE,FALSE,0);

    GtkWidget*desc_lbl=gtk_label_new("A modern terminal emulator with liquid glass aesthetics for Linux.");
    gtk_widget_set_halign(desc_lbl,GTK_ALIGN_CENTER);
    gtk_label_set_justify(GTK_LABEL(desc_lbl),GTK_JUSTIFY_CENTER);
    gtk_widget_set_sensitive(desc_lbl,FALSE);
    gtk_box_pack_start(GTK_BOX(vb),desc_lbl,FALSE,FALSE,0);

    GtkWidget*ver_lbl=gtk_label_new("Version 1.0.0");
    gtk_widget_set_halign(ver_lbl,GTK_ALIGN_CENTER);
    gtk_widget_set_sensitive(ver_lbl,FALSE);
    gtk_box_pack_start(GTK_BOX(vb),ver_lbl,FALSE,FALSE,0);

    gtk_box_pack_start(GTK_BOX(vb),gtk_separator_new(GTK_ORIENTATION_HORIZONTAL),FALSE,FALSE,4);

    GtkWidget*author_lbl=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(author_lbl),"<span weight='bold'>By Chokri Hammedi</span>");
    gtk_widget_set_halign(author_lbl,GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(vb),author_lbl,FALSE,FALSE,0);

    GtkWidget*copy_lbl=gtk_label_new("All rights reserved © 2026");
    gtk_widget_set_halign(copy_lbl,GTK_ALIGN_CENTER);
    gtk_widget_set_sensitive(copy_lbl,FALSE);
    gtk_box_pack_start(GTK_BOX(vb),copy_lbl,FALSE,FALSE,0);

    GtkWidget*gh_lbl=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(gh_lbl),"<a href=\"https://github.com/blue0x1/liquid-glass\">github.com/blue0x1/liquid-glass</a>");
    gtk_widget_set_halign(gh_lbl,GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(vb),gh_lbl,FALSE,FALSE,0);

    GtkWidget*spacer_bot=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
    gtk_widget_set_vexpand(spacer_bot,TRUE);
    gtk_box_pack_start(GTK_BOX(vb),spacer_bot,TRUE,TRUE,0);

    gtk_widget_show_all(win);
}
static void sw_about(GtkButton*b,gpointer d){(void)b;(void)d; open_about_impl();}
static void sw_destroyed(GtkWidget*w,gpointer d){
    (void)w;(void)d;
    G.sw_win=G.sw_opacity_scale=G.sw_opacity_label=G.sw_preset_combo=G.sw_color_btn=NULL;
}

static GtkWidget* sw_row(const char*lbl,GtkWidget*ctrl){
    GtkWidget*row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,12);
    GtkWidget*l=gtk_label_new(lbl);
    gtk_label_set_xalign(GTK_LABEL(l),0); gtk_widget_set_size_request(l,160,-1);
    gtk_box_pack_start(GTK_BOX(row),l,   FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(row),ctrl,TRUE, TRUE, 0);
    return row;
}

static void open_settings_impl(void){
    if(G.sw_win){gtk_window_present(GTK_WINDOW(G.sw_win));return;}

    GtkWidget*win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    G.sw_win=win;
    gtk_widget_set_name(win,"settings-window");
    GdkScreen*screen=gdk_screen_get_default();
    GdkVisual*rgba_vis=gdk_screen_get_rgba_visual(screen);
    if(rgba_vis){ gtk_widget_set_visual(win,rgba_vis); gtk_widget_set_app_paintable(win,TRUE); }
    gtk_window_set_title(GTK_WINDOW(win),"Settings");
    gtk_window_set_default_size(GTK_WINDOW(win),600,550);
    gtk_window_set_resizable(GTK_WINDOW(win),TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(win),GTK_WINDOW(G.window));
    gtk_window_set_position(GTK_WINDOW(win),GTK_WIN_POS_CENTER_ON_PARENT);
    g_signal_connect(win,"destroy",G_CALLBACK(sw_destroyed),NULL);
    g_signal_connect(win,"realize",G_CALLBACK(on_settings_realize),NULL);
    g_signal_connect(win,"draw",G_CALLBACK(on_clear_draw),NULL);

    GtkWidget*hbar=gtk_header_bar_new();
    gtk_widget_set_name(hbar,"settings-header");
    gtk_header_bar_set_title(GTK_HEADER_BAR(hbar),"Settings");
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(hbar),FALSE);
    GtkWidget*close_btn=gtk_button_new_with_label("×");
    gtk_widget_set_name(close_btn,"tab-close");
    gtk_widget_set_size_request(close_btn,12,12);
    gtk_button_set_relief(GTK_BUTTON(close_btn),GTK_RELIEF_NONE);
    g_signal_connect(close_btn,"clicked",G_CALLBACK(sw_close),NULL);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(hbar),close_btn);
    gtk_window_set_titlebar(GTK_WINDOW(win),hbar);

    GtkWidget*vb=gtk_box_new(GTK_ORIENTATION_VERTICAL,18);
    gtk_widget_set_name(vb,"settings-root");
    gtk_container_add(GTK_CONTAINER(win),vb);

    GtkWidget*t1=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(t1),"<span size='x-large' weight='bold'>Theme Color</span>");
    gtk_label_set_xalign(GTK_LABEL(t1),0); gtk_box_pack_start(GTK_BOX(vb),t1,FALSE,FALSE,0);

    GtkWidget*s1=gtk_label_new("Choose a preset or pick a custom color.");
    gtk_label_set_xalign(GTK_LABEL(s1),0); gtk_widget_set_sensitive(s1,FALSE);
    gtk_box_pack_start(GTK_BOX(vb),s1,FALSE,FALSE,0);

    GtkWidget*combo=gtk_combo_box_text_new(); G.sw_preset_combo=combo;
    int ai=0;
    for(int i=0;PRESETS[i].id;i++){
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo),PRESETS[i].label);
        if(!strcmp(G.cfg.preset,PRESETS[i].id))ai=i;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo),ai);
    g_signal_connect(combo,"changed",G_CALLBACK(sw_preset_changed),NULL);
    gtk_box_pack_start(GTK_BOX(vb),sw_row("Preset",combo),FALSE,FALSE,0);

    GtkWidget*color=gtk_color_button_new();
    G.sw_color_btn=color;
    gtk_color_button_set_title(GTK_COLOR_BUTTON(color),"Choose Theme Color");
    GdkRGBA cc; current_theme_color(&cc);
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(color),&cc);
    g_signal_connect(color,"color-set",G_CALLBACK(sw_color_changed),NULL);
    gtk_box_pack_start(GTK_BOX(vb),sw_row("Color",color),FALSE,FALSE,0);

    gtk_box_pack_start(GTK_BOX(vb),gtk_separator_new(GTK_ORIENTATION_HORIZONTAL),FALSE,FALSE,0);

    GtkWidget*t2=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(t2),"<span size='x-large' weight='bold'>Glass</span>");
    gtk_label_set_xalign(GTK_LABEL(t2),0); gtk_box_pack_start(GTK_BOX(vb),t2,FALSE,FALSE,0);

    GtkWidget*s2=gtk_label_new("Controls the transparency of the glass effect.");
    gtk_label_set_xalign(GTK_LABEL(s2),0); gtk_widget_set_sensitive(s2,FALSE);
    gtk_box_pack_start(GTK_BOX(vb),s2,FALSE,FALSE,0);

    GtkWidget*oprow=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,12);
    GtkWidget*oplbl_txt=gtk_label_new("Glass opacity");
    gtk_label_set_xalign(GTK_LABEL(oplbl_txt),0);
    gtk_widget_set_size_request(oplbl_txt,160,-1);
    GtkWidget*scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0,1,0.01);
    G.sw_opacity_scale=scale;
    gtk_range_set_value(GTK_RANGE(scale),G.cfg.glass_opacity);
    gtk_scale_set_draw_value(GTK_SCALE(scale),FALSE);
    GtkWidget*val_lbl=gtk_label_new(NULL); G.sw_opacity_label=val_lbl;
    {char s[16]; snprintf(s,sizeof(s),"%.2f",G.cfg.glass_opacity); gtk_label_set_text(GTK_LABEL(val_lbl),s);}
    gtk_widget_set_size_request(val_lbl,42,-1);
    g_signal_connect(scale,"value-changed",G_CALLBACK(sw_opacity_changed),NULL);
    gtk_box_pack_start(GTK_BOX(oprow),oplbl_txt,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(oprow),scale,     TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(oprow),val_lbl,   FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vb),oprow,FALSE,FALSE,0);

    gtk_box_pack_start(GTK_BOX(vb),gtk_separator_new(GTK_ORIENTATION_HORIZONTAL),FALSE,FALSE,0);

    GtkWidget*actions=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);
    GtkWidget*save=gtk_button_new_with_label("Save");
    g_signal_connect(save,"clicked",G_CALLBACK(sw_save),NULL);
    gtk_box_pack_start(GTK_BOX(actions),save,FALSE,FALSE,0);
    GtkWidget*about_btn=gtk_button_new_with_label("About");
    g_signal_connect(about_btn,"clicked",G_CALLBACK(sw_about),NULL);
    gtk_box_pack_end(GTK_BOX(actions),about_btn,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vb),actions,FALSE,FALSE,0);

    gtk_widget_show_all(win);
}

static gboolean on_key(GtkWidget*w,GdkEventKey*ev,gpointer d){
    (void)w;(void)d;
    gboolean ctrl =(ev->state&GDK_CONTROL_MASK)!=0;
    gboolean shift=(ev->state&GDK_SHIFT_MASK)  !=0;
    if(ctrl&&shift){
        if(ev->keyval==GDK_KEY_C||ev->keyval==GDK_KEY_c){
            if(G.active>=0)vte_terminal_copy_clipboard_format(VTE_TERMINAL(G.tabs[G.active].vte),VTE_FORMAT_TEXT);
            return TRUE;}
        if(ev->keyval==GDK_KEY_V||ev->keyval==GDK_KEY_v){
            if(G.active>=0)vte_terminal_paste_clipboard(VTE_TERMINAL(G.tabs[G.active].vte));
            return TRUE;}
    }
    if(ctrl&&!shift){
        if(ev->keyval==GDK_KEY_t||ev->keyval==GDK_KEY_T){tab_switch(tab_add());return TRUE;}
        if(ev->keyval==GDK_KEY_w||ev->keyval==GDK_KEY_W){tab_close(G.active); return TRUE;}
        if(ev->keyval==GDK_KEY_comma){open_settings_impl(); return TRUE;}
        if(ev->keyval==GDK_KEY_backslash){sidebar_toggle_impl();return TRUE;}
    }
    if(ctrl&&(ev->keyval==GDK_KEY_Tab||ev->keyval==GDK_KEY_ISO_Left_Tab)){
        int n=shift?G.active-1:G.active+1;
        if(n<0)n=G.ntabs-1; if(n>=G.ntabs)n=0;
        tab_switch(n); return TRUE;
    }
    return FALSE;
}

int main(int argc,char*argv[]){
    gtk_init(&argc,&argv);
    gdk_set_program_class("liquid_glass_gtk");
    memset(&G,0,sizeof(G));
    backdrop_refraction_init(&G.refract);
    {
        const char*desktop=getenv("XDG_CURRENT_DESKTOP");
        const char*session=getenv("DESKTOP_SESSION");
        gboolean plasma = (desktop && (strstr(desktop,"KDE") || strstr(desktop,"Plasma"))) ||
                          (session && strstr(session,"plasma"));
        backdrop_refraction_set_capture_enabled(&G.refract, !plasma);
    }
    G.active=-1;
    snprintf(G.exe_path,sizeof(G.exe_path),"%s",(argc>0&&argv[0])?argv[0]:"./liquid_glass_gtk");

    cfg_load(&G.cfg);
    theme_set(&G.theme,G.cfg.preset,G.cfg.custom_hex,G.cfg.glass_opacity);

    G.dyn_css=gtk_css_provider_new();
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(G.dyn_css),GTK_STYLE_PROVIDER_PRIORITY_APPLICATION+1);

    GtkCssProvider*css=gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,

      "window { background: transparent; }"
      "box    { background: transparent; }"
      "stack  { background: transparent; }"
      "paned  { background: transparent; }"

      "headerbar {"
      "  background: rgba(5,13,28,0.30);"
      "  border-bottom: 1px solid rgba(255,255,255,0.08);"
      "  color: rgba(220,222,235,1);"
      "}"
      "headerbar .title { font-size:0px; }"
      "headerbar button { background:transparent; border:none; color:rgba(220,222,235,0.9); }"
      "headerbar button:hover { background:rgba(255,255,255,0.12); border-radius:6px; }"
      "#header-icon-btn {"
      "  background:transparent; border:none; color:rgba(235,238,248,0.96);"
      "  text-shadow:0 1px 2px rgba(0,0,0,0.75); font-size:15px;"
      "  padding:2px 8px; border-radius:6px;"
      "}"
      "#header-icon-btn:hover { background:rgba(255,255,255,0.14); color:#ffffff; }"

      "#wm-close, #wm-min, #wm-max {"
      "  border:none; border-radius:6px; padding:0; margin:0;"
      "  min-width:12px; min-height:12px; font-size:0px;"
      "}"
      "#wm-close { background:#FF5F57; color:rgba(80,0,0,0.85); }"
      "#wm-close:hover { background:#FF3B30; }"
      "#wm-min { background:#FEBC2E; color:rgba(75,45,0,0.90); }"
      "#wm-min:hover { background:#F59F00; }"
      "#wm-max { background:#28C840; color:rgba(0,65,12,0.88); }"
      "#wm-max:hover { background:#1DB832; }"
      "#wm-close label, #wm-min label, #wm-max label { padding:0; margin:0; font-size:0px; }"

      "#tab-bar { min-height:38px; padding:4px 4px 2px 10px; }"
      "#tab-bar-wrap { background:transparent; }"

      "#tab-btn {"
      "  background:transparent; border:none; border-radius:8px;"
      "  color:rgba(200,205,225,0.55); font-size:12px; font-weight:500;"
      "  padding:4px 14px; min-width:72px;"
      "}"
      "#tab-btn.tab-active {"
      "  background:rgba(255,255,255,0.10);"
      "  color:rgba(220,222,235,1); font-weight:600;"
      "}"
      "#tab-btn:hover { background:rgba(255,255,255,0.06); }"

      "#tab-close {"
      "  background:transparent; border:none; border-radius:6px;"
      "  color:rgba(200,205,225,0.42); font-size:10px; font-weight:600;"
      "  padding:0; margin:6px 5px 6px 3px;"
      "  min-height:12px; min-width:12px;"
      "}"
      "#tab-close label { padding:0; margin:0; }"
      "#tab-close:hover { background:rgba(255,80,80,0.18); color:#ff7070; }"

      "#tab-add {"
      "  background:transparent; border:none; border-radius:7px;"
      "  color:rgba(200,205,225,0.50); font-size:16px; padding:0 8px;"
      "  min-height:0; min-width:0;"
      "}"
      "#tab-add:hover { background:rgba(255,255,255,0.10); color:rgba(220,222,235,1); }"

      "#sidebar { min-width:220px; }"

      "#sb-group-hdr { padding:8px 0 4px 0; }"
      "#sb-group-lbl { color:rgba(200,205,225,0.85); font-size:12px; font-weight:700; }"
      "#sb-group-cnt { color:rgba(200,205,225,0.45); font-size:10px; font-weight:700; }"

      "#sb-tab-row {"
      "  background:transparent; border:none; border-radius:12px;"
      "  padding:0; margin:1px 8px;"
      "}"
      "#sb-tab-row:hover { background:rgba(255,255,255,0.06); }"
      "#sb-tab-row-active {"
      "  background:rgba(255,255,255,0.10); border:none; border-radius:12px;"
      "  padding:0; margin:1px 8px;"
      "}"
      "#sb-tab-row-active:hover { background:rgba(255,255,255,0.14); }"
      "#sb-tab      { color:rgba(200,205,225,0.60); font-size:12px; padding:8px 0; }"
      "#sb-tab-active{ color:rgba(220,222,235,1.0); font-size:12px; font-weight:600; padding:8px 0; }"
      "#sb-tab-icon  { color:rgba(200,205,225,0.40); font-size:11px; }"

      "#sb-new-group {"
      "  background:transparent; border:none; border-radius:8px;"
      "  color:rgba(200,205,225,0.45); font-size:11px; margin:0 8px;"
      "}"
      "#sb-new-group:hover { background:rgba(255,255,255,0.07); }"
      "#sb-divider { margin:0 8px; }"

      "vte-terminal { border:none; }"

      "paned > separator { min-width:0; min-height:0; background:transparent; border:none; }"
      "paned separator { min-width:0; min-height:0; background:transparent; border:none; }"
      "GtkPaned { -GtkPaned-handle-size:0; }"

      "scrollbar { background:transparent; min-width:4px; }"
      "scrollbar slider { background:rgba(255,255,255,0.18); border-radius:3px; min-width:4px; min-height:24px; }"
      "scrollbar slider:hover { background:rgba(255,255,255,0.36); }"

      "#settings-window, #settings-window decoration, #settings-window box { background:transparent; }"
      "#settings-header { color:rgba(220,222,235,1); padding:2px 8px; }"
      "#settings-header .title { color:rgba(220,222,235,1); font-size:13px; font-weight:600; }"
      "#settings-header button { background:transparent; border:none; color:rgba(220,222,235,0.88); }"
      "#settings-header button:hover { background:rgba(255,255,255,0.12); border-radius:6px; }"
      "#settings-root { border:1px solid rgba(255,255,255,0.08); padding:24px; }"
      "#settings-root label { color:rgba(220,222,235,0.92); }"
      "#settings-root label:disabled { color:rgba(200,205,225,0.48); }"
      "#about-root { border:1px solid rgba(255,255,255,0.08); padding:24px; }"
      "#about-root label { color:rgba(220,222,235,0.92); }"
      "#about-root label:disabled { color:rgba(200,205,225,0.48); }"
      "#about-root separator { background:rgba(255,255,255,0.12); }"
      "#about-root a { color:rgba(130,180,255,0.90); }"
      "#settings-root entry, #settings-root combobox, #settings-root combobox box,"
      "#settings-root combobox button, #settings-root button {"
      "  background:rgba(255,255,255,0.08); border:1px solid rgba(255,255,255,0.12);"
      "  color:rgba(235,236,245,0.96); border-radius:7px;"
      "}"
      "#settings-root entry:focus, #settings-root combobox button:hover, #settings-root button:hover {"
      "  background:rgba(255,255,255,0.13); border-color:rgba(255,255,255,0.22);"
      "}"
      "#settings-root menu, #settings-root menuitem {"
      "  background:rgba(12,14,24,0.92); color:rgba(235,236,245,0.96);"
      "}"
      "#settings-root scale trough { background:rgba(255,255,255,0.10); border-radius:4px; min-height:5px; }"
      "#settings-root scale highlight { background:rgba(255,255,255,0.36); border-radius:4px; }"
      "#settings-root scale slider { background:rgba(235,236,245,0.92); border:none; min-width:14px; min-height:14px; }"
      "#settings-root separator { background:rgba(255,255,255,0.12); }"

      "menu, .context-menu {"
      "  background:rgba(12,14,24,0.88); border:1px solid rgba(255,255,255,0.12);"
      "  padding:4px; border-radius:8px;"
      "}"
      "menuitem {"
      "  background:transparent; color:rgba(235,236,245,0.94);"
      "  padding:6px 18px; border-radius:6px; font-size:12px;"
      "}"
      "menuitem:hover { background:rgba(255,255,255,0.12); color:#ffffff; }"
      "menu separator { background:rgba(255,255,255,0.12); min-height:1px; margin:4px 6px; }"

      ,-1,NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css),GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    dyn_css_update();

    GdkScreen*screen=gdk_screen_get_default();
    GdkVisual*rgba_vis=gdk_screen_get_rgba_visual(screen);

    GtkWidget*win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    G.window=win;
    gtk_window_set_title(GTK_WINDOW(win),"Liquid Glass");
    if(rgba_vis){ gtk_widget_set_visual(win,rgba_vis); gtk_widget_set_app_paintable(win,TRUE); }
    {
        GdkPixbuf*ico=load_icon_pixbuf(0);
        if(ico){ gtk_window_set_icon(GTK_WINDOW(win),ico); g_object_unref(ico); }
    }
    gtk_window_set_default_size(GTK_WINDOW(win),WIN_W,WIN_H);
    gtk_window_set_resizable(GTK_WINDOW(win),TRUE);
    g_signal_connect(win,"delete-event",   G_CALLBACK(gtk_main_quit),NULL);
    g_signal_connect(win,"key-press-event",G_CALLBACK(on_key),NULL);
    g_signal_connect(win,"window-state-event",G_CALLBACK(on_window_state),NULL);
    g_signal_connect(win,"realize",        G_CALLBACK(on_realize),NULL);
    g_signal_connect(win,"draw",           G_CALLBACK(window_draw),NULL);

    GtkWidget*hbar=gtk_header_bar_new();
    G.headerbar=hbar;
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(hbar),FALSE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(hbar),"");
    gtk_header_bar_set_decoration_layout(GTK_HEADER_BAR(hbar),":");
    gtk_window_set_titlebar(GTK_WINDOW(win),hbar);

    {
        GtkWidget*wctrl=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6);
        gtk_widget_set_margin_start(wctrl,8);
        gtk_widget_set_valign(wctrl,GTK_ALIGN_CENTER);

        GtkWidget*btn_close=gtk_button_new();
        gtk_widget_set_name(btn_close,"wm-close");
        gtk_widget_set_size_request(btn_close,12,12);
        gtk_widget_add_events(btn_close,GDK_ENTER_NOTIFY_MASK|GDK_LEAVE_NOTIFY_MASK);
        gtk_button_set_relief(GTK_BUTTON(btn_close),GTK_RELIEF_NONE);
        g_signal_connect_swapped(btn_close,"clicked",G_CALLBACK(gtk_main_quit),NULL);
        g_signal_connect(btn_close,"enter-notify-event",G_CALLBACK(on_wm_enter),GINT_TO_POINTER(0));
        g_signal_connect(btn_close,"leave-notify-event",G_CALLBACK(on_wm_leave),GINT_TO_POINTER(0));
        g_signal_connect_after(btn_close,"draw",G_CALLBACK(on_wm_button_draw),GINT_TO_POINTER(0));

        GtkWidget*btn_min=gtk_button_new();
        gtk_widget_set_name(btn_min,"wm-min");
        gtk_widget_set_size_request(btn_min,12,12);
        gtk_widget_add_events(btn_min,GDK_ENTER_NOTIFY_MASK|GDK_LEAVE_NOTIFY_MASK);
        gtk_button_set_relief(GTK_BUTTON(btn_min),GTK_RELIEF_NONE);
        g_signal_connect_swapped(btn_min,"clicked",G_CALLBACK(gtk_window_iconify),G.window);
        g_signal_connect(btn_min,"enter-notify-event",G_CALLBACK(on_wm_enter),GINT_TO_POINTER(1));
        g_signal_connect(btn_min,"leave-notify-event",G_CALLBACK(on_wm_leave),GINT_TO_POINTER(1));
        g_signal_connect_after(btn_min,"draw",G_CALLBACK(on_wm_button_draw),GINT_TO_POINTER(1));

        GtkWidget*btn_max=gtk_button_new();
        G.max_btn=btn_max;
        gtk_widget_set_name(btn_max,"wm-max");
        gtk_widget_set_size_request(btn_max,12,12);
        gtk_widget_add_events(btn_max,GDK_ENTER_NOTIFY_MASK|GDK_LEAVE_NOTIFY_MASK);
        gtk_button_set_relief(GTK_BUTTON(btn_max),GTK_RELIEF_NONE);
        g_signal_connect(btn_max,"clicked",G_CALLBACK(window_max_toggle),NULL);
        g_signal_connect(btn_max,"enter-notify-event",G_CALLBACK(on_wm_enter),GINT_TO_POINTER(2));
        g_signal_connect(btn_max,"leave-notify-event",G_CALLBACK(on_wm_leave),GINT_TO_POINTER(2));
        g_signal_connect_after(btn_max,"draw",G_CALLBACK(on_wm_button_draw),GINT_TO_POINTER(2));

        gtk_box_pack_start(GTK_BOX(wctrl),btn_close,FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(wctrl),btn_min,  FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(wctrl),btn_max,  FALSE,FALSE,0);
        gtk_header_bar_pack_start(GTK_HEADER_BAR(hbar),wctrl);
    }

    GtkWidget*sb_btn=gtk_button_new_with_label("☰");
    gtk_widget_set_name(sb_btn,"header-icon-btn");
    gtk_button_set_relief(GTK_BUTTON(sb_btn),GTK_RELIEF_NONE);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(hbar),sb_btn);
    g_signal_connect(sb_btn,"clicked",G_CALLBACK(sidebar_toggle),NULL);

    GtkWidget*gear=gtk_button_new_with_label("⚙");
    gtk_widget_set_name(gear,"header-icon-btn");
    gtk_button_set_relief(GTK_BUTTON(gear),GTK_RELIEF_NONE);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(hbar),gear);
    g_signal_connect(gear,"clicked",G_CALLBACK(open_settings),NULL);

    GtkWidget*root=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
    G.root=root;
    gtk_widget_set_name(root,"root");
    g_signal_connect(root,"size-allocate",G_CALLBACK(on_layout_changed),NULL);
    gtk_container_add(GTK_CONTAINER(win),root);

    GtkWidget*tab_bar_wrap=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
    G.tab_bar_wrap=tab_bar_wrap;
    gtk_widget_set_name(tab_bar_wrap,"tab-bar-wrap");
    g_signal_connect(tab_bar_wrap,"size-allocate",G_CALLBACK(on_layout_changed),NULL);

    GtkWidget*tab_bar=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,2);
    G.tab_bar=tab_bar;
    gtk_widget_set_name(tab_bar,"tab-bar");
    g_signal_connect(tab_bar,"size-allocate",G_CALLBACK(on_layout_changed),NULL);
    gtk_box_pack_start(GTK_BOX(tab_bar_wrap),tab_bar,FALSE,FALSE,0);

    GtkWidget*tab_sep=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    gtk_widget_set_name(tab_sep,"tab-bar-bottom");
    gtk_widget_set_size_request(tab_sep,-1,1);
    gtk_box_pack_start(GTK_BOX(tab_bar_wrap),tab_sep,FALSE,FALSE,0);

    gtk_box_pack_start(GTK_BOX(root),tab_bar_wrap,FALSE,FALSE,0);

    GtkWidget*add_btn=gtk_button_new_with_label("+");
    gtk_widget_set_name(add_btn,"tab-add");
    gtk_box_pack_start(GTK_BOX(tab_bar),add_btn,FALSE,FALSE,0);
    g_signal_connect(add_btn,"clicked",G_CALLBACK(on_add_tab),NULL);

    GtkWidget*hpaned=gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    G.hpaned=hpaned;
    gtk_box_pack_start(GTK_BOX(root),hpaned,TRUE,TRUE,0);

    GtkWidget*sidebar=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
    G.sidebar_box=sidebar;
    gtk_widget_set_name(sidebar,"sidebar");
    gtk_widget_set_size_request(sidebar,SIDEBAR_W,-1);
    g_signal_connect(sidebar,"size-allocate",G_CALLBACK(on_layout_changed),NULL);

    GtkWidget*sb_tabs_lbl=gtk_label_new("Tabs");
    {
        GtkStyleContext*ctx=gtk_widget_get_style_context(sb_tabs_lbl);
        GtkCssProvider*lp=gtk_css_provider_new();
        gtk_css_provider_load_from_data(lp,
            "label { color:rgba(220,222,235,0.90); font-size:12px; font-weight:600;"
            " padding:8px 12px 6px 12px; }",-1,NULL);
        gtk_style_context_add_provider(ctx,GTK_STYLE_PROVIDER(lp),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(lp);
    }
    gtk_label_set_xalign(GTK_LABEL(sb_tabs_lbl),0);
    gtk_box_pack_start(GTK_BOX(sidebar),sb_tabs_lbl,FALSE,FALSE,0);

    GtkWidget*sb_div=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
    gtk_widget_set_name(sb_div,"sidebar-divider");
    gtk_widget_set_size_request(sb_div,1,-1);

    GtkWidget*sb_wrap=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    G.sidebar_wrap=sb_wrap;
    g_signal_connect(sb_wrap,"size-allocate",G_CALLBACK(on_layout_changed),NULL);
    gtk_box_pack_start(GTK_BOX(sb_wrap),sidebar,TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(sb_wrap),sb_div,  FALSE,FALSE,0);

    gtk_paned_pack1(GTK_PANED(hpaned),sb_wrap,FALSE,FALSE);

    if(!G.cfg.show_sidebar){
        gtk_widget_set_no_show_all(G.sidebar_wrap,TRUE);
        gtk_widget_hide(G.sidebar_wrap);
    }

    GtkWidget*terminal_overlay=gtk_overlay_new();
    G.terminal_overlay=terminal_overlay;
    gtk_widget_set_name(terminal_overlay,"terminal-overlay");
    gtk_widget_set_hexpand(terminal_overlay,TRUE);
    gtk_widget_set_vexpand(terminal_overlay,TRUE);

    GtkWidget*gl_area=gtk_gl_area_new();
    G.gl_area=gl_area;
    gtk_widget_set_name(gl_area,"liquid-gl");
    gtk_widget_set_hexpand(gl_area,TRUE);
    gtk_widget_set_vexpand(gl_area,TRUE);
    gtk_gl_area_set_has_alpha(GTK_GL_AREA(gl_area),TRUE);
    gtk_gl_area_set_required_version(GTK_GL_AREA(gl_area),3,3);
    g_signal_connect(gl_area,"realize",G_CALLBACK(gl_area_realize),NULL);
    g_signal_connect(gl_area,"unrealize",G_CALLBACK(gl_area_unrealize),NULL);
    g_signal_connect(gl_area,"render",G_CALLBACK(gl_area_render),NULL);
    gtk_widget_add_tick_callback(gl_area,gl_tick,NULL,NULL);
    gtk_container_add(GTK_CONTAINER(terminal_overlay),gl_area);

    GtkWidget*stack=gtk_stack_new();
    G.stack=stack;
    gtk_widget_set_hexpand(stack,TRUE); gtk_widget_set_vexpand(stack,TRUE);
    gtk_stack_set_transition_type(GTK_STACK(stack),GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(stack),100);
    g_signal_connect(stack,"size-allocate",G_CALLBACK(on_layout_changed),NULL);
    gtk_overlay_add_overlay(GTK_OVERLAY(terminal_overlay),stack);
    gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(terminal_overlay),stack,FALSE);
    gtk_paned_pack2(GTK_PANED(hpaned),terminal_overlay,TRUE,FALSE);

    tab_switch(tab_add());

    {
        GdkDisplay*disp=gdk_screen_get_display(screen);
        GdkMonitor*mon=gdk_display_get_primary_monitor(disp);
        GdkRectangle geo; gdk_monitor_get_geometry(mon,&geo);
        gtk_window_move(GTK_WINDOW(win),geo.x+(geo.width-WIN_W)/2,geo.y+(geo.height-WIN_H)/2);
    }

    gtk_widget_show_all(win);
    if(!G.cfg.show_sidebar){
        gtk_widget_hide(G.sidebar_wrap);
        gtk_paned_set_position(GTK_PANED(G.hpaned),0);
    }

    g_idle_add(sidebar_show_idle,NULL);
    gtk_main();
    return 0;
}
