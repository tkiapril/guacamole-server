
/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "config.h"
#include "drv.h"
#include "composite.h"
#include "crtc.h"
#include "cursor.h"
#include "display.h"
#include "gc.h"
#include "log.h"
#include "pixmap.h"
#include "screen.h"
#include "window.h"

#include <xorg-server.h>
#include <xf86.h>
#include <xf86str.h>

#include <fb.h>
#include <micmap.h>
#include <mipointer.h>

#include <stdarg.h>
#include <time.h>

static DevPrivateKeyRec __GUAC_SCREEN_PRIVATE;

const DevPrivateKey GUAC_SCREEN_PRIVATE = &__GUAC_SCREEN_PRIVATE;

Bool guac_drv_pre_init(ScrnInfoPtr screen, int flags) {

    ClockRangePtr clock_range;
    Gamma ZERO_GAMMA = { 0.0, 0.0, 0.0 };
    rgb ZERO_RGB = { 0, 0, 0 };

    /* Set monitor */
    screen->monitor = screen->confScreen->monitor;

    /* Set screen depth */
    if (!xf86SetDepthBpp(screen, 0, 0, 0, Support32bppFb))
        return FALSE;

    /* Print display depth */
    xf86PrintDepthBpp(screen);

    /* Set weight */
    if (!xf86SetWeight(screen, ZERO_RGB, ZERO_RGB))
        return FALSE;

    /* Set default visual */
    if (!xf86SetDefaultVisual(screen, -1))
        return FALSE;

    /* Set gamma */
    if (!xf86SetGamma(screen, ZERO_GAMMA))
        return FALSE;

    /* VRAM (in kilobytes) */
    screen->videoRam = GUAC_DRV_VRAM;

    /* Clock range */
    screen->progClock = TRUE;
    clock_range = xnfcalloc(sizeof(ClockRange), 1);
    clock_range->next = NULL;
    clock_range->ClockMulFactor = 1;
    clock_range->ClockDivFactor = 1;
    clock_range->minClock = 10000;
    clock_range->maxClock = 400000;
    clock_range->clockIndex = -1;
    clock_range->interlaceAllowed = FALSE;
    clock_range->doubleScanAllowed = FALSE;

    /* Validate modes */
    if (xf86ValidateModes(screen,
                screen->monitor->Modes, screen->display->modes,
                clock_range, NULL,
                128, 2048, /* minPitch, maxPitch */
                8,         /* pitchInc */
                128, 2048, /* minHeight, maxHeight */
                screen->display->virtualX,
                screen->display->virtualY,
                screen->videoRam * 1024,
                LOOKUP_BEST_REFRESH) == -1)
        return FALSE;

    /* Prune any invalid modes */
    xf86PruneDriverModes(screen);

    /* Allocate screen */
    guac_drv_screen* guac_screen = malloc(sizeof(guac_drv_screen));
    screen->driverPrivate = guac_screen;

    /* Set CRTC config handlers */
    xf86CrtcConfigInit(screen, &guac_drv_crtc_configfuncs);

    /* Init screen size limits */
    xf86CrtcSetSizeRange(screen, 256, 256, 2048, 2048);

    /* Allocate CRTC and corresponding output */
    guac_screen->crtc = xf86CrtcCreate(screen, &guac_drv_crtc_funcs);
    guac_screen->output = xf86OutputCreate(screen,
            &guac_drv_output_funcs, "guac-0");

    guac_screen->output->possible_crtcs = 2; /* 1 << total_number_of_crtcs */
    guac_screen->output->possible_clones = 0;

    /* Use first mode available */
    screen->currentMode = screen->modes;

    /* Set CRTC parameters */
    xf86InitialConfiguration(screen, TRUE);

    /* Print modes being used */
    xf86PrintModes(screen);

    /* Set DPI */
    xf86SetDpi(screen, 0, 0);

    /* Load framebuffer module for managing screen */
    if (!xf86LoadSubModule(screen, "fb"))
        return FALSE;

    xf86DrvMsg(screen->scrnIndex, X_INFO, "PreInit complete\n");
    return TRUE;

}

Bool guac_drv_switch_mode(ScrnInfoPtr screen_info, DisplayModePtr mode) {
    /* STUB */
    guac_drv_log(GUAC_LOG_DEBUG, "guac_drv_switch_mode");
    return TRUE;
}

void guac_drv_adjust_frame(ScrnInfoPtr screen_info, int x, int y) {
    /* STUB */
    guac_drv_log(GUAC_LOG_DEBUG, "guac_drv_adjust_frame");
}

Bool guac_drv_enter_vt(ScrnInfoPtr screen_info) {
    /* STUB */
    guac_drv_log(GUAC_LOG_DEBUG, "guac_drv_enter_vt");
    return TRUE;
}

void guac_drv_leave_vt(ScrnInfoPtr screen_info) {
    /* STUB */
    guac_drv_log(GUAC_LOG_DEBUG, "guac_drv_leave_vt");
}

static Bool guac_drv_close_screen(ScreenPtr screen) {

    /* Get guac_drv_screen */
    guac_drv_screen* guac_screen = 
        (guac_drv_screen*) dixGetPrivate(&(screen->devPrivates),
                                         GUAC_SCREEN_PRIVATE);

    /* STUB */
    guac_drv_log(GUAC_LOG_DEBUG, "STUB: %s", __func__);

    /* Call wrapped function */
    if (guac_screen->wrapped_close_screen)
        return guac_screen->wrapped_close_screen(screen);

    return TRUE;

}

static Bool guac_drv_create_window(WindowPtr window) {

    Bool ret = TRUE;

    /* Get guac_drv_screen */
    ScreenPtr screen = window->drawable.pScreen;
    guac_drv_screen* guac_screen = 
        (guac_drv_screen*) dixGetPrivate(&(screen->devPrivates),
                                         GUAC_SCREEN_PRIVATE);

    guac_drv_drawable* drawable;
    guac_drv_drawable* parent;
    int x, y, z;
    int width, height;
    int opacity;

    /* Get window rect */
    x = window->origin.x;
    y = window->origin.y;
    width  = window->drawable.width;
    height = window->drawable.height;

    /* If root window, no parent, index is 0 */
    if (window->parent == NULL)
        parent = NULL;

    /* Otherwise, find parent */
    else
        parent = (guac_drv_drawable*)
            dixGetPrivate(&(window->parent->devPrivates), GUAC_WINDOW_PRIVATE);

    /* Set opacity based on whether window is visible */
    if (window->realized)
        opacity = 0xFF;
    else
        opacity = 0;

    /* If first window among siblings, assign Z of 0 */
    if (window->nextSib == NULL)
        z = 0;

    /* Otherwise, choose nearest sibling's Z+1 */
    else {

        /* Get sibling drawable */
        guac_drv_drawable* sibling_drawable = (guac_drv_drawable*)
            dixGetPrivate(&(window->nextSib->devPrivates), GUAC_WINDOW_PRIVATE);

        z = sibling_drawable->layer->surface->z + 1;

    }

    /* Assign index and parent */
    drawable = guac_drv_display_create_layer(guac_screen->display,
            parent, x, y, z, width, height, opacity);

    if (parent != NULL)
        guac_drv_log(GUAC_LOG_DEBUG, "Create window 0x%x %ix%i+%i+%i "
                "(z=%i, opacity=%i) -> layer %i, parent=0x%x (layer %i)",
                window->drawable.id, width, height, x, y,
                z, opacity, drawable->layer->layer->index,
                window->parent->drawable.id,
                parent->layer->layer->index);
    else
        guac_drv_log(GUAC_LOG_DEBUG, "Create window 0x%x %ix%i+%i+%i "
                "(z=%i, opacity=%i) -> layer %i, parent=ROOT/NULL",
                window->drawable.id, width, height, x, y,
                z, opacity, drawable->layer->layer->index);

    /* Store drawable */
    dixSetPrivate(&(window->devPrivates), GUAC_WINDOW_PRIVATE, drawable);

    /* Update clients */
    guac_drv_display_touch(guac_screen->display);

    /* Call wrapped function */
    if (guac_screen->wrapped_create_window) {
        GUAC_DRV_UNWRAP(screen->CreateWindow,
                        guac_screen->wrapped_create_window);

        ret = screen->CreateWindow(window);

        GUAC_DRV_WRAP(screen->CreateWindow,
                      guac_screen->wrapped_create_window,
                      guac_drv_create_window);
    }

    return ret;

}

static Bool guac_drv_change_window_attributes(WindowPtr window,
        unsigned long mask) {

    Bool ret = TRUE;

    /* Get guac_drv_screen */
    ScreenPtr screen = window->drawable.pScreen;
    guac_drv_screen* guac_screen = 
        (guac_drv_screen*) dixGetPrivate(&(screen->devPrivates),
                                         GUAC_SCREEN_PRIVATE);

    /* Call wrapped function */
    if (guac_screen->wrapped_change_window_attributes) {
        GUAC_DRV_UNWRAP(screen->ChangeWindowAttributes,
                        guac_screen->wrapped_change_window_attributes);

        ret = screen->ChangeWindowAttributes(window, mask);

        GUAC_DRV_WRAP(screen->ChangeWindowAttributes,
                      guac_screen->wrapped_change_window_attributes,
                      guac_drv_change_window_attributes);
    }

    return ret;

}

static Bool guac_drv_create_gc(GCPtr gc) {

    Bool ret = TRUE;

    /* Get guac_drv_screen */
    ScreenPtr screen = gc->pScreen;
    guac_drv_screen* guac_screen = 
        (guac_drv_screen*) dixGetPrivate(&(screen->devPrivates),
                                         GUAC_SCREEN_PRIVATE);

    /* Call wrapped function */
    if (guac_screen->wrapped_create_gc) {
        GUAC_DRV_UNWRAP(screen->CreateGC,
                        guac_screen->wrapped_create_gc);

        ret = screen->CreateGC(gc);
        if (ret) {
            gc->ops = &guac_drv_gcops;
            dixSetPrivate(&(gc->devPrivates), GUAC_GC_PRIVATE, guac_screen);
        }

        GUAC_DRV_WRAP(screen->CreateGC,
                      guac_screen->wrapped_create_gc,
                      guac_drv_create_gc);
    }

    return ret;

}

static Bool guac_drv_unrealize_window(WindowPtr window) {

    /* Get guac_drv_screen */
    ScreenPtr screen = window->drawable.pScreen;
    guac_drv_screen* guac_screen = 
        (guac_drv_screen*) dixGetPrivate(&(screen->devPrivates),
                                         GUAC_SCREEN_PRIVATE);

    /* Get drawable */
    guac_drv_drawable* drawable =
        (guac_drv_drawable*) dixGetPrivate(&(window->devPrivates),
                                         GUAC_WINDOW_PRIVATE);

    /* Set invisible */
    guac_drv_drawable_shade(drawable, 0);
    guac_drv_display_touch(guac_screen->display);

    guac_drv_log(GUAC_LOG_DEBUG, "Unrealize window 0x%x "
            "opacity -> %i (layer %i)",
            window->drawable.id, 0x0,
            drawable->layer->layer->index);

    /* Call wrapped function */
    if (guac_screen->wrapped_unrealize_window)
        return guac_screen->wrapped_unrealize_window(window);

    return TRUE;

}

static Bool guac_drv_realize_window(WindowPtr window) {

    /* Get guac_drv_screen */
    ScreenPtr screen = window->drawable.pScreen;
    guac_drv_screen* guac_screen = 
        (guac_drv_screen*) dixGetPrivate(&(screen->devPrivates),
                                         GUAC_SCREEN_PRIVATE);
    /* Get drawable */
    guac_drv_drawable* drawable = 
        (guac_drv_drawable*) dixGetPrivate(&(window->devPrivates),
                                         GUAC_WINDOW_PRIVATE);

    /* Set visible */
    guac_drv_drawable_shade(drawable, 0xFF);
    guac_drv_display_touch(guac_screen->display);

    guac_drv_log(GUAC_LOG_DEBUG, "Realize window 0x%x "
            "opacity -> %i (layer %i)",
            window->drawable.id, 0xFF,
            drawable->layer->layer->index);

    /* Call wrapped function */
    if (guac_screen->wrapped_realize_window)
        return guac_screen->wrapped_realize_window(window);

    return TRUE;

}

static void guac_drv_move_window(WindowPtr window, int x, int y,
        WindowPtr sibling, VTKind kind) {

    /* Get guac_drv_screen */
    ScreenPtr screen = window->drawable.pScreen;
    guac_drv_screen* guac_screen = 
        (guac_drv_screen*) dixGetPrivate(&(screen->devPrivates),
                                         GUAC_SCREEN_PRIVATE);

    /* Get drawable */
    guac_drv_drawable* drawable =
        (guac_drv_drawable*) dixGetPrivate(&(window->devPrivates),
                                         GUAC_WINDOW_PRIVATE);

    /* Update window */
    guac_drv_drawable_move(drawable, x, y);
    guac_drv_display_touch(guac_screen->display);

    guac_drv_log(GUAC_LOG_DEBUG, "Move window 0x%x +%i+%i "
            "(layer %i)",
            window->drawable.id, x, y,
            drawable->layer->layer->index);

    /* Call wrapped function */
    if (guac_screen->wrapped_move_window)
        guac_screen->wrapped_move_window(window, x, y, sibling, kind);

}

static void guac_drv_clear_outside(WindowPtr window) {

    /* Get drawable */
    guac_drv_drawable* drawable =
        (guac_drv_drawable*) dixGetPrivate(&(window->devPrivates),
                                         GUAC_WINDOW_PRIVATE);

    /* Produce box encompassing entire drawable */
    BoxRec bounds = {
        .x1 = 0,
        .y1 = 0,
        .x2 = drawable->layer->surface->width,
        .y2 = drawable->layer->surface->height
    };

    /* Create region from inverse of bounding shape */
    RegionPtr region = RegionCreate(NullBox, 1);
    RegionInverse(region, &window->borderClip, &bounds);

    if (window->optional != NULL && window->optional->boundingShape != NULL)
        RegionInverse(region, window->optional->boundingShape, &bounds);

    /* Clear outside bounding shape */
    GUAC_DRV_DRAWABLE_CLIP(drawable, (DrawablePtr) window, region,
        guac_drv_drawable_clear, drawable, 0, 0,
        drawable->layer->surface->width,
        drawable->layer->surface->height);

    /* Destroy temporary region */
    RegionDestroy(region);

}

static void guac_drv_resize_window(WindowPtr window, int x, int y,
        unsigned int w, unsigned int h, WindowPtr sibling) {

    /* Get guac_drv_screen */
    ScreenPtr screen = window->drawable.pScreen;
    guac_drv_screen* guac_screen = 
        (guac_drv_screen*) dixGetPrivate(&(screen->devPrivates),
                                         GUAC_SCREEN_PRIVATE);

    /* Get drawable */
    guac_drv_drawable* drawable =
        (guac_drv_drawable*) dixGetPrivate(&(window->devPrivates),
                                         GUAC_WINDOW_PRIVATE);

    /* Update window */
    guac_drv_drawable_move(drawable, x, y);
    guac_drv_drawable_resize(drawable, w, h);
    guac_drv_clear_outside(window);
    guac_drv_display_touch(guac_screen->display);

    guac_drv_log(GUAC_LOG_DEBUG, "Resize window 0x%x %ix%i+%i+%i "
            "(layer %i)",
            window->drawable.id, w, h, x, y,
            drawable->layer->layer->index);

    /* Call wrapped function */
    if (guac_screen->wrapped_resize_window)
        guac_screen->wrapped_resize_window(window, x, y, w, h, sibling);

}

static void guac_drv_reparent_window(WindowPtr window,
        WindowPtr prior_parent) {

    /* Get guac_drv_screen */
    ScreenPtr screen = window->drawable.pScreen;
    guac_drv_screen* guac_screen = 
        (guac_drv_screen*) dixGetPrivate(&(screen->devPrivates),
                                         GUAC_SCREEN_PRIVATE);

    /* Get drawable */
    guac_drv_drawable* drawable =
        (guac_drv_drawable*) dixGetPrivate(&(window->devPrivates),
                                         GUAC_WINDOW_PRIVATE);

    /* Update window parent */
    if (window->parent != NULL) {

        /* Get parent drawable */
        guac_drv_drawable* parent_drawable =
            (guac_drv_drawable*) dixGetPrivate(&(window->parent->devPrivates),
                                             GUAC_WINDOW_PRIVATE);

        /* Assign parent */
        guac_drv_drawable_reparent(drawable, parent_drawable);
        guac_drv_display_touch(guac_screen->display);

        guac_drv_log(GUAC_LOG_DEBUG, "Reparent window "
                "0x%x (layer %i) -> 0x%x (layer %i)",
                window->drawable.id, drawable->layer->layer->index,
                window->parent->drawable.id,
                parent_drawable->layer->layer->index);

    }
    else
        guac_drv_log(GUAC_LOG_DEBUG, "Unhandled reparent (to root?) "
                "0x%x (layer %i)",
                window->drawable.id, drawable->layer->layer->index);

    /* Call wrapped function */
    if (guac_screen->wrapped_reparent_window)
        guac_screen->wrapped_reparent_window(window, prior_parent);

}

static void guac_drv_restack_window(WindowPtr window, WindowPtr old_next) {

    /* Get guac_drv_screen */
    ScreenPtr screen = window->drawable.pScreen;
    guac_drv_screen* guac_screen = 
        (guac_drv_screen*) dixGetPrivate(&(screen->devPrivates),
                                         GUAC_SCREEN_PRIVATE);

    /* Find bottom of stack */
    WindowPtr bottom_sib = window;
    while (bottom_sib->nextSib != NULL)
        bottom_sib = bottom_sib->nextSib;

    /* Get associated guac window */
    guac_drv_drawable* bottom_drawable = (guac_drv_drawable*)
        dixGetPrivate(&(bottom_sib->devPrivates), GUAC_WINDOW_PRIVATE);

    int last_z = bottom_drawable->layer->surface->z;

    /* Correct stacking order for all windows in stack */
    WindowPtr current = bottom_sib->prevSib;
    while (current != NULL) {

        /* Get current window */
        guac_drv_drawable* cur_drawable = (guac_drv_drawable*)
            dixGetPrivate(&(current->devPrivates), GUAC_WINDOW_PRIVATE);

        /* Set stacking order if necessary */
        if (cur_drawable->layer->surface->z <= last_z) {
            guac_drv_drawable_stack(cur_drawable, last_z+1);
            guac_drv_display_touch(guac_screen->display);

            guac_drv_log(GUAC_LOG_DEBUG, "Restack "
                    "0x%x (layer %i) z -> %i",
                    current->drawable.id, cur_drawable->layer->layer->index,
                    last_z+1);

        }

        /* Advance to next window up */
        last_z = cur_drawable->layer->surface->z;
        current = current->prevSib;

    }

    /* Call wrapped function */
    if (guac_screen->wrapped_restack_window) {
        GUAC_DRV_UNWRAP(screen->RestackWindow,
                        guac_screen->wrapped_restack_window);

        screen->RestackWindow(window, old_next);

        GUAC_DRV_WRAP(screen->RestackWindow,
                      guac_screen->wrapped_restack_window,
                      guac_drv_restack_window);
    }

}

static void guac_drv_set_shape(WindowPtr window, int kind) {

    /* Get guac_drv_screen */
    ScreenPtr screen = window->drawable.pScreen;
    guac_drv_screen* guac_screen =
        (guac_drv_screen*) dixGetPrivate(&(screen->devPrivates),
                                         GUAC_SCREEN_PRIVATE);

    /* Clear region outside border */
    guac_drv_clear_outside(window);
    guac_drv_display_touch(guac_screen->display);

    guac_drv_log(GUAC_LOG_DEBUG, "Set shape 0x%x (kind=%i)",
            window->drawable.id, kind);

    /* Call wrapped function */
    if (guac_screen->wrapped_set_shape)
        guac_screen->wrapped_set_shape(window, kind);

}

static Bool guac_drv_destroy_window(WindowPtr window) {

    Bool ret = TRUE;

    /* Get guac_drv_screen */
    ScreenPtr screen = window->drawable.pScreen;
    guac_drv_screen* guac_screen = 
        (guac_drv_screen*) dixGetPrivate(&(screen->devPrivates),
                                         GUAC_SCREEN_PRIVATE);

    /* Get drawable */
    guac_drv_drawable* drawable =
        (guac_drv_drawable*) dixGetPrivate(&(window->devPrivates),
                                         GUAC_WINDOW_PRIVATE);

    /* If window defined, remove and handle */
    if (drawable != NULL) {

        /* Remove from private */
        dixSetPrivate(&(window->devPrivates), GUAC_WINDOW_PRIVATE, NULL);

        guac_drv_log(GUAC_LOG_DEBUG, "Destroy window 0x%x (layer %i)",
                window->drawable.id, drawable->layer->layer->index);

        /* Destroy drawable */
        guac_drv_display_destroy_layer(guac_screen->display, drawable);
        guac_drv_display_touch(guac_screen->display);

    }

    /* Call wrapped function */
    if (guac_screen->wrapped_destroy_window) {
        GUAC_DRV_UNWRAP(screen->DestroyWindow,
                        guac_screen->wrapped_destroy_window);

        ret = screen->DestroyWindow(window);

        GUAC_DRV_WRAP(screen->DestroyWindow,
                      guac_screen->wrapped_destroy_window,
                      guac_drv_destroy_window);
    }

    return ret;

}

static Bool guac_drv_save_screen(ScreenPtr screen, int mode) {
    /* STUB */
    return TRUE;
}

void guac_drv_free_screen(ScrnInfoPtr screen_info) {

    /* Get guac_drv_screen */
    ScreenPtr screen = screen_info->pScreen;
    guac_drv_screen* guac_screen = 
        (guac_drv_screen*) dixGetPrivate(&(screen->devPrivates),
                                         GUAC_SCREEN_PRIVATE);

    /* Free framebuffer */
    free(guac_screen->framebuffer);

    /* Free private data */
    free(guac_screen);

}

Bool guac_drv_screen_init(ScreenPtr screen, int argc, char** argv) {

    ScrnInfoPtr screen_info = xf86Screens[screen->myNum];
    guac_drv_screen* guac_screen;

    /* Register private keys */
    dixRegisterPrivateKey(GUAC_SCREEN_PRIVATE, PRIVATE_SCREEN, 0);
    dixRegisterPrivateKey(GUAC_WINDOW_PRIVATE, PRIVATE_WINDOW, 0);
    dixRegisterPrivateKey(GUAC_GC_PRIVATE,     PRIVATE_GC,     0);

    /* Pull allocated guac_drv_screen from screen info */
    guac_screen = (guac_drv_screen*) screen_info->driverPrivate;
    dixSetPrivate(&(screen->devPrivates), GUAC_SCREEN_PRIVATE, guac_screen);

    /* Allocate framebuffer */
    guac_screen->framebuffer = malloc(screen_info->videoRam * 1024);

    miClearVisualTypes();
    if (!miSetVisualTypes(screen_info->depth,
                miGetDefaultVisualMask(screen_info->depth),
                screen_info->rgbBits, screen_info->defaultVisual))
        return FALSE;

    if (!miSetPixmapDepths())
        return FALSE;

    /* Init framebuffer */
    if (!fbScreenInit(screen, guac_screen->framebuffer,
                screen_info->virtualX, screen_info->virtualY,
                screen_info->xDpi, screen_info->yDpi,
                screen_info->displayWidth, screen_info->bitsPerPixel))
        return FALSE;

    /* Backing store supported */
    screen->backingStoreSupport = Always;

    /* Set RGB offset/mask */
    if (screen_info->depth > 8) {

        int i;
        VisualPtr visual = screen->visuals;

        for (i=0; i<screen->numVisuals; i++) {

            if ((visual->class | DynamicClass) == DirectColor) {
                visual->offsetRed   = screen_info->offset.red;
                visual->offsetGreen = screen_info->offset.green;
                visual->offsetBlue  = screen_info->offset.blue;
                visual->redMask     = screen_info->mask.red;
                visual->greenMask   = screen_info->mask.green;
                visual->blueMask    = screen_info->mask.blue;
            }

            visual++;
        }

    }

    fbPictureInit(screen, NULL, 0);

    xf86SetBlackWhitePixels(screen);

    miDCInitialize(screen, xf86GetPointerScreenFuncs());
    if (!miCreateDefColormap(screen))
        return FALSE;

    /* Init cursor */
    if (!guac_drv_init_cursor(screen))
        return FALSE;

    screen->width = screen_info->currentMode->HDisplay;
    screen->height = screen_info->currentMode->VDisplay;

    xf86CrtcScreenInit(screen);

    /* Init options to defaults */
    OptionInfoRec options[GUAC_DRV_OPTIONINFOREC_SIZE];
    memcpy(options, GUAC_OPTIONS, sizeof(GUAC_OPTIONS));

    /* Read options from xorg.conf */
    xf86CollectOptions(screen_info, NULL);
    xf86ProcessOptions(screen_info->scrnIndex, screen_info->options, options);

    /* Set log level if overridden in xorg.conf */
    const char* log_level_str = options[GUAC_DRV_OPTION_LOG_LEVEL].value.str;
    if (strcmp(log_level_str, "error") == 0)
        guac_drv_log_level = GUAC_LOG_ERROR;
    else if (strcmp(log_level_str, "warning") == 0)
        guac_drv_log_level = GUAC_LOG_WARNING;
    else if (strcmp(log_level_str, "info") == 0)
        guac_drv_log_level = GUAC_LOG_INFO;
    else if (strcmp(log_level_str, "debug") == 0)
        guac_drv_log_level = GUAC_LOG_DEBUG;
    else if (strcmp(log_level_str, "trace") == 0)
        guac_drv_log_level = GUAC_LOG_TRACE;
    else
        guac_drv_log(GUAC_LOG_WARNING, "Invalid log level: \"%s\". Using "
                "defaults.", log_level_str);

    /* Init display */
#ifdef ENABLE_SSL
    const char* cert_file = options[GUAC_DRV_OPTION_SSL_CERT_FILE].value.str;
    const char* key_file = options[GUAC_DRV_OPTION_SSL_KEY_FILE].value.str;
#else
    const char* cert_file = NULL;
    const char* key_file = NULL;
#endif

    guac_screen->display = guac_drv_display_alloc(screen,
            options[GUAC_DRV_OPTION_LISTEN_ADDRESS].value.str,
            options[GUAC_DRV_OPTION_LISTEN_PORT].value.str,
            options[GUAC_DRV_OPTION_PULSE_AUDIO_SERVER_NAME].value.str,
            cert_file,
            key_file);

    screen->SaveScreen = guac_drv_save_screen;

    guac_screen->wrapped_close_screen = screen->CloseScreen;
    screen->CloseScreen = guac_drv_close_screen;

    guac_screen->wrapped_create_window = screen->CreateWindow;
    screen->CreateWindow = guac_drv_create_window;

    guac_screen->wrapped_change_window_attributes = screen->ChangeWindowAttributes;
    screen->ChangeWindowAttributes = guac_drv_change_window_attributes;

    guac_screen->wrapped_create_gc = screen->CreateGC;
    screen->CreateGC = guac_drv_create_gc;

    guac_screen->wrapped_realize_window = screen->RealizeWindow;
    screen->RealizeWindow = guac_drv_realize_window;

    guac_screen->wrapped_unrealize_window = screen->UnrealizeWindow;
    screen->UnrealizeWindow = guac_drv_unrealize_window;

    guac_screen->wrapped_move_window = screen->MoveWindow;
    screen->MoveWindow = guac_drv_move_window;

    guac_screen->wrapped_resize_window = screen->ResizeWindow;
    screen->ResizeWindow = guac_drv_resize_window;

    guac_screen->wrapped_reparent_window = screen->ReparentWindow;
    screen->ReparentWindow = guac_drv_reparent_window;

    guac_screen->wrapped_restack_window = screen->RestackWindow;
    screen->RestackWindow = guac_drv_restack_window;

    guac_screen->wrapped_set_shape = screen->SetShape;
    screen->SetShape = guac_drv_set_shape;

    guac_screen->wrapped_destroy_window = screen->DestroyWindow;
    screen->DestroyWindow = guac_drv_destroy_window;

    PictureScreenPtr pict_screen = GetPictureScreenIfSet(screen);
    if (pict_screen != NULL) {
        guac_screen->wrapped_composite = pict_screen->Composite;
        pict_screen->Composite = guac_drv_composite;
    }

    return TRUE;
}

ModeStatus guac_drv_valid_mode(ScrnInfoPtr screen_info, DisplayModePtr mode,
        Bool verbose, int flags) {
    return MODE_OK;
}

