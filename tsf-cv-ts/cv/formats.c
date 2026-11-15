/** @file
 * @brief CV Group
 *
 * The same scene is written in several containers and pixel formats -
 * JPEG, BMP, TIFF, an 8-bit single-channel PNG, and a 16-bit 3-channel
 * PNG - and tapi_cv_info reads each one's container, channel count and
 * bit depth back. This exercises the format sniffing and the depth and
 * channel reporting across more than the one 8-bit PNG the other tests
 * use. The reported bit depth is charted.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "cv/formats"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_cv.h"
#include "tsapi_cv.h"

/* One fixture and what tapi_cv_info must report for it. */
typedef struct variant {
    const char *file;       /* fixture name */
    const char *format;     /* expected container */
    int channels;           /* expected channel count */
    int depth;              /* expected bits per channel */
} variant;

int
main(int argc, char **argv)
{
    tsapi_cv_session sess;
    te_mi_logger *logger = NULL;
    tapi_cv_image_info info;
    unsigned int i;
    static const variant variants[] = {
        { "scene.jpg",   "JPEG", 3, 8  },
        { "scene.bmp",   "BMP",  3, 8  },
        { "scene.tiff",  "TIFF", 3, 8  },
        { "gray.png",    "PNG",  1, 8  },
        { "deep16.png",  "PNG",  3, 16 },
    };

    TEST_START;

    TEST_STEP("Open a session and generate the fixtures");
    CHECK_RC(tsapi_cv_session_init(&sess, "pco_cv_formats"));
    if (!tapi_cv_available(sess.pco))
        TEST_SKIP("The agent has no OpenCV");
    rc = tsapi_cv_fixtures(&sess);
    if (rc != 0)
    {
        if (TE_RC_GET_ERROR(rc) == TE_EOPNOTSUPP)
            TEST_SKIP("The agent cannot generate fixtures (no python3+cv2)");
        CHECK_RC(rc);
    }

    CHECK_RC(te_mi_logger_meas_create("tsf-cv", &logger));

    TEST_STEP("Each container and pixel format is reported correctly");
    for (i = 0; i < TE_ARRAY_LEN(variants); i++)
    {
        const variant *v = &variants[i];
        te_string path = TE_STRING_INIT;

        tsapi_cv_fixture(&sess, v->file, &path);
        CHECK_RC(tapi_cv_info(sess.pco, path.ptr, &info));
        RING("%s: %dx%d, %d ch, %d-bit, %s", v->file, info.width, info.height,
             info.channels, info.depth, info.format);

        if (info.width != TSAPI_CV_SCENE_W || info.height != TSAPI_CV_SCENE_H)
        {
            tapi_cv_image_info_free(&info);
            te_string_free(&path);
            TEST_VERDICT("%s is %dx%d, expected %dx%d", v->file, info.width,
                         info.height, TSAPI_CV_SCENE_W, TSAPI_CV_SCENE_H);
        }
        if (info.channels != v->channels)
        {
            int got = info.channels;

            tapi_cv_image_info_free(&info);
            te_string_free(&path);
            TEST_VERDICT("%s has %d channels, expected %d", v->file, got,
                         v->channels);
        }
        if (info.depth != v->depth)
        {
            int got = info.depth;

            tapi_cv_image_info_free(&info);
            te_string_free(&path);
            TEST_VERDICT("%s is %d-bit, expected %d-bit", v->file, got,
                         v->depth);
        }
        if (info.format == NULL || strcmp(info.format, v->format) != 0)
        {
            char *got = info.format != NULL ? strdup(info.format) : NULL;

            tapi_cv_image_info_free(&info);
            te_string_free(&path);
            TEST_VERDICT("%s sniffed as %s, expected %s", v->file,
                         got != NULL ? got : "?", v->format);
        }

        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "variant", TE_MI_MEAS_AGGR_SINGLE, i,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "depth", TE_MI_MEAS_AGGR_SINGLE, info.depth,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        tapi_cv_image_info_free(&info);
        te_string_free(&path);
    }

    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "depth", "Bit depth by format variant");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "depth",
                               TE_MI_GRAPH_AXIS_X, "variant");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "depth", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "depth");
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    tsapi_cv_session_fini(&sess);
    TEST_END;
}
