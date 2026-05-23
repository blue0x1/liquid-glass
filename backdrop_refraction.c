/*
 * Liquid Glass Terminal
 * Author: Chokri Hammedi (blue0x1)
 * https://github.com/blue0x1/liquid-glass
 * License: MIT
 * Copyright (C) 2026 Chokri Hammedi. All rights reserved.
 */

#include "backdrop_refraction.h"

#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static GLuint shader_new(GLenum type,const char*src){
    GLuint sh=glCreateShader(type);
    glShaderSource(sh,1,&src,NULL);
    glCompileShader(sh);
    GLint ok=0;
    glGetShaderiv(sh,GL_COMPILE_STATUS,&ok);
    if(!ok){
        char log[1024];
        GLsizei len=0;
        glGetShaderInfoLog(sh,sizeof(log),&len,log);
        fprintf(stderr,"liquid-glass: OpenGL shader compile failed: %.*s\n",(int)len,log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static gboolean build_program(BackdropRefraction*r){
    if(r->program)return TRUE;

    static const char*vs_src=
        "#version 330 core\n"
        "layout(location=0) in vec2 a_pos;\n"
        "out vec2 v_uv;\n"
        "void main(){\n"
        "  v_uv=a_pos*0.5+0.5;\n"
        "  gl_Position=vec4(a_pos,0.0,1.0);\n"
        "}\n";

    static const char*fs_src=
        "#version 330 core\n"
        "in vec2 v_uv;\n"
        "out vec4 frag;\n"
        "uniform sampler2D u_backdrop;\n"
        "uniform vec2 u_resolution;\n"
        "uniform vec2 u_texture_size;\n"
        "uniform float u_time;\n"
        "uniform vec4 u_theme;\n"
        "uniform float u_opacity;\n"
        "uniform int u_has_backdrop;\n"
        "float hash(vec2 p){\n"
        "  return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453123);\n"
        "}\n"
        "float noise(vec2 p){\n"
        "  vec2 i=floor(p),f=fract(p);\n"
        "  f=f*f*(3.0-2.0*f);\n"
        "  float a=hash(i),b=hash(i+vec2(1.0,0.0));\n"
        "  float c=hash(i+vec2(0.0,1.0)),d=hash(i+vec2(1.0,1.0));\n"
        "  return mix(mix(a,b,f.x),mix(c,d,f.x),f.y);\n"
        "}\n"
        "vec3 blur_backdrop(vec2 uv,vec2 px,float strength){\n"
        "  if(u_has_backdrop==0){\n"
        "    return u_theme.rgb*0.82+vec3(0.018,0.024,0.034);\n"
        "  }\n"
        "  vec2 texel=1.0/max(u_texture_size,vec2(1.0));\n"
        "  vec3 c=texture(u_backdrop,uv).rgb*0.20;\n"
        "  c+=texture(u_backdrop,uv+vec2( 1.0, 0.0)*texel*strength).rgb*0.10;\n"
        "  c+=texture(u_backdrop,uv+vec2(-1.0, 0.0)*texel*strength).rgb*0.10;\n"
        "  c+=texture(u_backdrop,uv+vec2( 0.0, 1.0)*texel*strength).rgb*0.10;\n"
        "  c+=texture(u_backdrop,uv+vec2( 0.0,-1.0)*texel*strength).rgb*0.10;\n"
        "  c+=texture(u_backdrop,uv+vec2( 0.707, 0.707)*texel*strength*0.8).rgb*0.08;\n"
        "  c+=texture(u_backdrop,uv+vec2(-0.707, 0.707)*texel*strength*0.8).rgb*0.08;\n"
        "  c+=texture(u_backdrop,uv+vec2( 0.707,-0.707)*texel*strength*0.8).rgb*0.08;\n"
        "  c+=texture(u_backdrop,uv+vec2(-0.707,-0.707)*texel*strength*0.8).rgb*0.08;\n"
        "  c+=texture(u_backdrop,uv+vec2( 1.0, 0.0)*texel*strength*2.0).rgb*0.035;\n"
        "  c+=texture(u_backdrop,uv+vec2(-1.0, 0.0)*texel*strength*2.0).rgb*0.035;\n"
        "  c+=texture(u_backdrop,uv+vec2( 0.0, 1.0)*texel*strength*2.0).rgb*0.035;\n"
        "  c+=texture(u_backdrop,uv+vec2( 0.0,-1.0)*texel*strength*2.0).rgb*0.035;\n"
        "  return c;\n"
        "}\n"
        "void main(){\n"
        "  vec2 uv=v_uv;\n"
        "  vec2 tex_uv=vec2(uv.x,1.0-uv.y);\n"
        "  vec2 aspect=vec2(u_resolution.x/max(u_resolution.y,1.0),1.0);\n"
        "  vec2 p=(uv-0.5)*aspect;\n"
        "  float t=u_time;\n"
        "  float radius=length(p);\n"
        "  float rim=pow(smoothstep(0.34,0.96,radius),2.55);\n"
        "  float inner=1.0-smoothstep(0.72,1.08,radius);\n"
        "  float lens=(noise(p*4.8+vec2(0.0,t*0.010))-0.5)*0.010;\n"
        "  lens+=(noise(p*15.0-vec2(t*0.008,0.0))-0.5)*0.0035;\n"
        "  vec2 normal=normalize(p+0.0001);\n"
        "  vec2 refract_uv=clamp(tex_uv+vec2(normal.x,-normal.y)*(rim*0.030+lens),vec2(0.002),vec2(0.998));\n"
        "  vec2 red_uv=clamp(refract_uv+normal*rim*0.006,vec2(0.002),vec2(0.998));\n"
        "  vec2 blue_uv=clamp(refract_uv-normal*rim*0.006,vec2(0.002),vec2(0.998));\n"
        "  vec3 glass=blur_backdrop(refract_uv,normal,9.0+rim*18.0);\n"
        "  if(u_has_backdrop!=0){\n"
        "    glass.r=blur_backdrop(red_uv,normal,9.0+rim*18.0).r;\n"
        "    glass.b=blur_backdrop(blue_uv,normal,9.0+rim*18.0).b;\n"
        "  }\n"
        "  float luma=dot(glass,vec3(0.2126,0.7152,0.0722));\n"
        "  vec3 tint=u_theme.rgb;\n"
        "  glass=mix(vec3(luma),glass,0.58);\n"
        "  glass=mix(glass,tint*0.78+vec3(0.030,0.038,0.052),0.34+u_opacity*0.22);\n"
        "  float frost=noise(refract_uv*u_resolution.xy*0.040)+noise(refract_uv*u_resolution.xy*0.120)*0.35;\n"
        "  float topGlow=smoothstep(0.94,0.08,uv.y)*0.17;\n"
        "  float lowerShade=smoothstep(0.30,1.0,uv.y)*0.10;\n"
        "  float caustic=(noise(refract_uv*u_resolution.xy*0.010+vec2(t*0.015,0.0))-0.5)*0.050*inner;\n"
        "  vec3 sheen=vec3(0.78,0.90,1.0)*(topGlow+rim*0.12+caustic);\n"
        "  vec3 grain=vec3(frost-0.55)*0.020;\n"
        "  vec3 col=glass+sheen+grain-lowerShade;\n"
        "  float alpha=mix(0.20,0.66,u_opacity)+topGlow*0.14+rim*0.16+caustic*0.08;\n"
        "  frag=vec4(col,clamp(alpha,0.18,0.84));\n"
        "}\n";

    GLuint vs=shader_new(GL_VERTEX_SHADER,vs_src);
    GLuint fs=shader_new(GL_FRAGMENT_SHADER,fs_src);
    if(!vs||!fs)return FALSE;

    r->program=glCreateProgram();
    glAttachShader(r->program,vs);
    glAttachShader(r->program,fs);
    glLinkProgram(r->program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok=0;
    glGetProgramiv(r->program,GL_LINK_STATUS,&ok);
    if(!ok){
        char log[1024];
        GLsizei len=0;
        glGetProgramInfoLog(r->program,sizeof(log),&len,log);
        fprintf(stderr,"liquid-glass: OpenGL program link failed: %.*s\n",(int)len,log);
        glDeleteProgram(r->program);
        r->program=0;
        return FALSE;
    }

    const GLfloat verts[]={
        -1.0f,-1.0f,  1.0f,-1.0f, -1.0f, 1.0f,
         1.0f,-1.0f,  1.0f, 1.0f, -1.0f, 1.0f
    };
    glGenVertexArrays(1,&r->vao);
    glBindVertexArray(r->vao);
    glGenBuffers(1,&r->vbo);
    glBindBuffer(GL_ARRAY_BUFFER,r->vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(verts),verts,GL_STATIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(GLfloat),(void*)0);
    glEnableVertexAttribArray(0);
    glGenTextures(1,&r->backdrop_tex);
    glBindTexture(GL_TEXTURE_2D,r->backdrop_tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glBindVertexArray(0);
    return TRUE;
}

static int mask_shift(unsigned long mask){
    int shift=0;
    if(!mask)return 0;
    while((mask&1)==0){mask>>=1;shift++;}
    return shift;
}

static int mask_bits(unsigned long mask){
    int bits=0;
    while(mask){
        if(mask&1)bits++;
        mask>>=1;
    }
    return bits;
}

static unsigned char scale_masked(unsigned long pixel,unsigned long mask,int shift,int bits){
    if(!mask||bits<=0)return 0;
    unsigned long v=(pixel&mask)>>shift;
    unsigned long max=(1UL<<bits)-1UL;
    return (unsigned char)((v*255UL)/max);
}

static Window find_root_child(Display*dpy,Window root,Window win){
    Window cur=win;
    while(cur!=root&&cur!=None){
        Window root_ret,parent_ret;
        Window*children=NULL;
        unsigned int n=0;
        if(!XQueryTree(dpy,cur,&root_ret,&parent_ret,&children,&n))return None;
        if(children)XFree(children);
        if(parent_ret==root)return cur;
        cur=parent_ret;
    }
    return None;
}

static gboolean compositor_running(Display*dpy,int screen){
    char sel_name[32];
    snprintf(sel_name,sizeof(sel_name),"_NET_WM_CM_%d",screen);
    Atom sel=XInternAtom(dpy,sel_name,False);
    return XGetSelectionOwner(dpy,sel)!=None;
}

static gboolean capture_composited_backdrop(
    Display*dpy,Window root,Window self,int cap_x,int cap_y,int cap_w,int cap_h,
    unsigned char**rgba_out)
{
    int composite_event=0, composite_error=0;
    if(!XCompositeQueryExtension(dpy,&composite_event,&composite_error)){
        return FALSE;
    }

    int screen=DefaultScreen(dpy);
    if(!compositor_running(dpy,screen)){
        return FALSE;
    }

    Window root_ret=0,parent_ret=0;
    Window*children=NULL;
    unsigned int nchildren=0;
    if(!XQueryTree(dpy,root,&root_ret,&parent_ret,&children,&nchildren)){
        return FALSE;
    }

    Window frame=find_root_child(dpy,root,self);
    if(frame==None){
        if(children)XFree(children);
        return FALSE;
    }
    gboolean seen_self=FALSE;
    Pixmap dst=XCreatePixmap(dpy,root,(unsigned int)cap_w,(unsigned int)cap_h,(unsigned int)DefaultDepth(dpy,screen));
    if(!dst){
        if(children)XFree(children);
        return FALSE;
    }

    XRenderPictFormat*dst_fmt=XRenderFindVisualFormat(dpy,DefaultVisual(dpy,screen));
    if(!dst_fmt){
        XFreePixmap(dpy,dst);
        if(children)XFree(children);
        return FALSE;
    }

    Picture dst_pic=XRenderCreatePicture(dpy,dst,dst_fmt,0,NULL);
    if(!dst_pic){
        XFreePixmap(dpy,dst);
        if(children)XFree(children);
        return FALSE;
    }

    XRenderColor clear={0,0,0,0};
    Picture clear_pic=XRenderCreateSolidFill(dpy,&clear);
    if(clear_pic){
        XRenderComposite(dpy,PictOpSrc,clear_pic,None,dst_pic,0,0,0,0,0,0,(unsigned int)cap_w,(unsigned int)cap_h);
        XRenderFreePicture(dpy,clear_pic);
    }

    for(unsigned int i=0;i<nchildren;i++){
        Window win=children[i];
        if(win==frame){
            seen_self=TRUE;
            break;
        }

        XWindowAttributes wa;
        if(!XGetWindowAttributes(dpy,win,&wa))continue;
        if(wa.map_state!=IsViewable || wa.width<1 || wa.height<1)continue;

        int wx=wa.x;
        int wy=wa.y;
        int wr=wx+wa.width;
        int wb=wy+wa.height;
        int ix1=cap_x>wx?cap_x:wx;
        int iy1=cap_y>wy?cap_y:wy;
        int ix2=(cap_x+cap_w)<wr?(cap_x+cap_w):wr;
        int iy2=(cap_y+cap_h)<wb?(cap_y+cap_h):wb;
        if(ix1>=ix2 || iy1>=iy2)continue;

        GdkDisplay*_gdisp=gdk_display_get_default();
        gdk_x11_display_error_trap_push(_gdisp);
        Pixmap src_pix=XCompositeNameWindowPixmap(dpy,win);
        XSync(dpy,False);
        if(gdk_x11_display_error_trap_pop(_gdisp)||!src_pix)continue;

        XRenderPictFormat*src_fmt=XRenderFindVisualFormat(dpy,wa.visual);
        if(!src_fmt){
            XFreePixmap(dpy,src_pix);
            continue;
        }

        Picture src_pic=XRenderCreatePicture(dpy,src_pix,src_fmt,0,NULL);
        if(!src_pic){
            XFreePixmap(dpy,src_pix);
            continue;
        }

        int src_x=ix1-wx;
        int src_y=iy1-wy;
        int dst_x=ix1-cap_x;
        int dst_y=iy1-cap_y;
        XRenderComposite(dpy,PictOpOver,src_pic,None,dst_pic,
            src_x,src_y,0,0,dst_x,dst_y,(unsigned int)(ix2-ix1),(unsigned int)(iy2-iy1));

        XRenderFreePicture(dpy,src_pic);
        XFreePixmap(dpy,src_pix);
    }

    if(children)XFree(children);
    XRenderFreePicture(dpy,dst_pic);
    if(!seen_self){
        XFreePixmap(dpy,dst);
        return FALSE;
    }

    XSync(dpy,False);
    XImage*img=XGetImage(dpy,dst,0,0,(unsigned int)cap_w,(unsigned int)cap_h,AllPlanes,ZPixmap);
    XFreePixmap(dpy,dst);
    if(!img){
        return FALSE;
    }

    int rs=mask_shift(img->red_mask),gs=mask_shift(img->green_mask),bs=mask_shift(img->blue_mask);
    int rb=mask_bits(img->red_mask),gb=mask_bits(img->green_mask),bb=mask_bits(img->blue_mask);
    unsigned char*rgba=g_malloc((size_t)cap_w*(size_t)cap_h*4);
    memset(rgba,0,(size_t)cap_w*(size_t)cap_h*4);
    for(int y=0;y<cap_h;y++){
        for(int x=0;x<cap_w;x++){
            unsigned long p=XGetPixel(img,x,y);
            size_t i=((size_t)y*(size_t)cap_w+(size_t)x)*4;
            rgba[i+0]=scale_masked(p,img->red_mask,rs,rb);
            rgba[i+1]=scale_masked(p,img->green_mask,gs,gb);
            rgba[i+2]=scale_masked(p,img->blue_mask,bs,bb);
            rgba[i+3]=255;
        }
    }
    XDestroyImage(img);
    *rgba_out=rgba;
    return TRUE;
}

static void update_backdrop_texture(BackdropRefraction*r,GtkWidget*window,GtkWidget*surface,int w,int h,int scale){
    gint64 now=g_get_monotonic_time();
    if(scale<1)scale=1;
    if(now-r->last_capture_us<66000&&r->tex_w==w&&r->tex_h==h&&r->capture_scale==scale)return;
    r->last_capture_us=now;
    r->has_backdrop=FALSE;

    GdkWindow*wg=gtk_widget_get_window(window);
    GdkWindow*sg=gtk_widget_get_window(surface);
    if(!wg||!sg||!GDK_IS_X11_WINDOW(wg)){
        static gboolean warned=FALSE;
        if(!warned){
            fprintf(stderr,"liquid-glass: backdrop refraction needs an X11 GDK window; using shader fallback.\n");
            warned=TRUE;
        }
        return;
    }

    int ox=0,oy=0,wx=0,wy=0;
    gdk_window_get_origin(wg,&ox,&oy);
    if(!gtk_widget_translate_coordinates(surface,window,0,0,&wx,&wy))return;

    Display*dpy=GDK_WINDOW_XDISPLAY(wg);
    Window root=DefaultRootWindow(dpy);
    int screen=DefaultScreen(dpy);
    int sw=DisplayWidth(dpy,screen);
    int sh=DisplayHeight(dpy,screen);
    int sx=ox+wx*scale;
    int sy=oy+wy*scale;
    int cap_x=sx<0?0:sx;
    int cap_y=sy<0?0:sy;
    int cap_r=(sx+w)>sw?sw:(sx+w);
    int cap_b=(sy+h)>sh?sh:(sy+h);
    int cap_w=cap_r-cap_x;
    int cap_h=cap_b-cap_y;
    if(cap_w<1||cap_h<1)return;

    unsigned char*rgba=NULL;
    gboolean captured=FALSE;
    if(compositor_running(dpy,screen)){
        captured=capture_composited_backdrop(dpy,root,GDK_WINDOW_XID(wg),cap_x,cap_y,cap_w,cap_h,&rgba);
    }

    if(!captured){
        XImage*img=XGetImage(dpy,root,cap_x,cap_y,(unsigned int)cap_w,(unsigned int)cap_h,AllPlanes,ZPixmap);
        if(!img)return;

        int rs=mask_shift(img->red_mask),gs=mask_shift(img->green_mask),bs=mask_shift(img->blue_mask);
        int rb=mask_bits(img->red_mask),gb=mask_bits(img->green_mask),bb=mask_bits(img->blue_mask);
        rgba=g_malloc((size_t)w*(size_t)h*4);
        memset(rgba,0,(size_t)w*(size_t)h*4);
        int dst_x=cap_x-sx;
        int dst_y=cap_y-sy;
        for(int y=0;y<h;y++){
            int iy=y-dst_y;
            if(iy<0||iy>=cap_h)continue;
            for(int x=0;x<w;x++){
                int ix=x-dst_x;
                if(ix<0||ix>=cap_w)continue;
                unsigned long p=XGetPixel(img,ix,iy);
                size_t i=((size_t)y*(size_t)w+(size_t)x)*4;
                rgba[i+0]=scale_masked(p,img->red_mask,rs,rb);
                rgba[i+1]=scale_masked(p,img->green_mask,gs,gb);
                rgba[i+2]=scale_masked(p,img->blue_mask,bs,bb);
                rgba[i+3]=255;
            }
        }
        XDestroyImage(img);
    } else if(rgba && (cap_w!=w || cap_h!=h)){
        unsigned char*resized=g_malloc((size_t)w*(size_t)h*4);
        memset(resized,0,(size_t)w*(size_t)h*4);
        int dst_x=cap_x-sx;
        int dst_y=cap_y-sy;
        for(int y=0;y<h;y++){
            int iy=y-dst_y;
            if(iy<0||iy>=cap_h)continue;
            for(int x=0;x<w;x++){
                int ix=x-dst_x;
                if(ix<0||ix>=cap_w)continue;
                size_t src_i=((size_t)iy*(size_t)cap_w+(size_t)ix)*4;
                size_t dst_i=((size_t)y*(size_t)w+(size_t)x)*4;
                resized[dst_i+0]=rgba[src_i+0];
                resized[dst_i+1]=rgba[src_i+1];
                resized[dst_i+2]=rgba[src_i+2];
                resized[dst_i+3]=255;
            }
        }
        g_free(rgba);
        rgba=resized;
    }

    if(!rgba)return;

    glBindTexture(GL_TEXTURE_2D,r->backdrop_tex);
    if(r->tex_w!=w||r->tex_h!=h){
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
        r->tex_w=w;
        r->tex_h=h;
    } else {
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
    }
    r->capture_scale=scale;
    g_free(rgba);
    r->has_backdrop=TRUE;
}

void backdrop_refraction_init(BackdropRefraction*r){
    memset(r,0,sizeof(*r));
    r->capture_backdrop=TRUE;
}

void backdrop_refraction_set_capture_enabled(BackdropRefraction*r,gboolean enabled){
    r->capture_backdrop=enabled;
}

void backdrop_refraction_realize(BackdropRefraction*r,GtkGLArea*area){
    gtk_gl_area_make_current(area);
    if(gtk_gl_area_get_error(area))return;
    r->start_us=g_get_monotonic_time();
    build_program(r);
}

void backdrop_refraction_unrealize(BackdropRefraction*r,GtkGLArea*area){
    gtk_gl_area_make_current(area);
    if(gtk_gl_area_get_error(area))return;
    if(r->backdrop_tex){glDeleteTextures(1,&r->backdrop_tex);r->backdrop_tex=0;}
    if(r->vbo){glDeleteBuffers(1,&r->vbo);r->vbo=0;}
    if(r->vao){glDeleteVertexArrays(1,&r->vao);r->vao=0;}
    if(r->program){glDeleteProgram(r->program);r->program=0;}
    r->has_backdrop=FALSE;
    r->tex_w=0;
    r->tex_h=0;
}

gboolean backdrop_refraction_render(BackdropRefraction*r,GtkGLArea*area,
    GtkWidget*window,GtkWidget*surface,double red,double green,double blue,
    double theme_alpha,double opacity){
    if(gtk_gl_area_get_error(area))return FALSE;
    if(!build_program(r))return FALSE;

    GtkAllocation a;
    gtk_widget_get_allocation(GTK_WIDGET(area),&a);
    if(a.width<1||a.height<1)return FALSE;
    int scale=gtk_widget_get_scale_factor(GTK_WIDGET(area));
    if(scale<1)scale=1;
    int fb_w=a.width*scale;
    int fb_h=a.height*scale;

    if(r->capture_backdrop){
        update_backdrop_texture(r,window,surface,fb_w,fb_h,scale);
    } else {
        r->has_backdrop=FALSE;
    }

    glViewport(0,0,fb_w,fb_h);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);

    float t=(float)((g_get_monotonic_time()-r->start_us)/1000000.0);
    glUseProgram(r->program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,r->backdrop_tex);
    glUniform1i(glGetUniformLocation(r->program,"u_backdrop"),0);
    glUniform2f(glGetUniformLocation(r->program,"u_resolution"),(float)fb_w,(float)fb_h);
    glUniform2f(glGetUniformLocation(r->program,"u_texture_size"),(float)r->tex_w,(float)r->tex_h);
    glUniform1f(glGetUniformLocation(r->program,"u_time"),t);
    glUniform4f(glGetUniformLocation(r->program,"u_theme"),
        (float)red,(float)green,(float)blue,(float)theme_alpha);
    glUniform1f(glGetUniformLocation(r->program,"u_opacity"),(float)opacity);
    glUniform1i(glGetUniformLocation(r->program,"u_has_backdrop"),r->has_backdrop?1:0);
    glBindVertexArray(r->vao);
    glDrawArrays(GL_TRIANGLES,0,6);
    glBindVertexArray(0);
    return TRUE;
}
