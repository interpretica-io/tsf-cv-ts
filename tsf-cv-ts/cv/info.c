/** @file
 * @brief CV Group
 *
 * Reading an image's dimensions and encoding back, and checking they
 * are what the fixture generator drew: the synthetic scene is exactly
 * 320x240, 3-channel, 8-bit PNG.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "cv/info"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_cv.h"
#include "tsapi_cv.h"

int
main(int argc, char **argv)
{
    tsapi_cv_session sess;
    tapi_cv_image_info info;
    te_mi_logger *logger = NULL;
    unsigned int i;
    static const char * const images[] = {
        "scene.png", "scene_diff.png", "text.png",
    };

    TEST_START;

    TEST_STEP("Open a session and generate the fixtures");
    CHECK_RC(tsapi_cv_session_init(&sess, "pco_cv_info"));
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

    TEST_STEP("The scene has the dimensions the generator drew");
    {
        te_string path = TE_STRING_INIT;

        tsapi_cv_fixture(&sess, "scene.png", &path);
        CHECK_RC(tapi_cv_info(sess.pco, path.ptr, &info));
        RING("scene.png: %dx%d, %d ch, %d-bit, %s", info.width, info.height,
             info.channels, info.depth, info.format);
        if (info.width != TSAPI_CV_SCENE_W || info.height != TSAPI_CV_SCENE_H)
            TEST_VERDICT("scene.png is %dx%d, expected %dx%d", info.width,
                         info.height, TSAPI_CV_SCENE_W, TSAPI_CV_SCENE_H);
        if (info.channels != 3)
            TEST_VERDICT("scene.png has %d channels, expected 3",
                         info.channels);
        if (info.depth != 8)
            TEST_VERDICT("scene.png is %d-bit, expected 8", info.depth);
        tapi_cv_image_info_free(&info);
        te_string_free(&path);
    }

    TEST_STEP("Read every fixture and chart its size");
    for (i = 0; i < TE_ARRAY_LEN(images); i++)
    {
        te_string path = TE_STRING_INIT;

        tsapi_cv_fixture(&sess, images[i], &path);
        CHECK_RC(tapi_cv_info(sess.pco, path.ptr, &info));

        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "image", TE_MI_MEAS_AGGR_SINGLE, i,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "width", TE_MI_MEAS_AGGR_SINGLE, info.width,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "height", TE_MI_MEAS_AGGR_SINGLE, info.height,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        tapi_cv_image_info_free(&info);
        te_string_free(&path);
    }

    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "dims", "Image dimensions (px)");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "dims",
                               TE_MI_GRAPH_AXIS_X, "image");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "dims", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "width");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "dims", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "height");
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    tsapi_cv_session_fini(&sess);
    TEST_END;
}
