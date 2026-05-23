/*
 * Liquid Glass Terminal
 * Author: Chokri Hammedi (blue0x1)
 * https://github.com/blue0x1/liquid-glass
 * License: MIT
 * Copyright (C) 2026 Chokri Hammedi. All rights reserved.
 */

#ifndef BACKDROP_REFRACTION_H
#define BACKDROP_REFRACTION_H

#include <gtk/gtk.h>
#include <epoxy/gl.h>

typedef struct {
    GLuint program;
    GLuint vbo;
    GLuint vao;
    GLuint backdrop_tex;
    gint64 start_us;
    gint64 last_capture_us;
    int tex_w;
    int tex_h;
    int capture_scale;
    gboolean capture_backdrop;
    gboolean has_backdrop;
} BackdropRefraction;

void backdrop_refraction_init(BackdropRefraction*r);
void backdrop_refraction_set_capture_enabled(BackdropRefraction*r,gboolean enabled);
void backdrop_refraction_realize(BackdropRefraction*r,GtkGLArea*area);
void backdrop_refraction_unrealize(BackdropRefraction*r,GtkGLArea*area);
gboolean backdrop_refraction_render(BackdropRefraction*r,GtkGLArea*area,
    GtkWidget*window,GtkWidget*surface,double red,double green,double blue,
    double theme_alpha,double opacity);

#endif
