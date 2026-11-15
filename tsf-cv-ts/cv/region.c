/** @file
 * @brief CV Group
 *
 * Measuring a region and testing a colour. The green "indicator"
 * rectangle is almost entirely inside a green HSV range; the red
 * rectangle is almost entirely outside it. The in-range share is
 * charted across the indicator, the red block and the whole scene -
 * the "is the light on?" check.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "cv/region"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_cv.h"
#include "tsapi_cv.h"

/* A green range in OpenCV HSV: hue around 60, well saturated and bright. */
static const tapi_cv_hsv_range green = {
    .h_lo = 40, .s_lo = 80, .v_lo = 80,
    .h_hi = 80, .s_hi = 255, .v_hi = 255,
};

int
main(int argc, char **argv)
{
    tsapi_cv_session sess;
    te_mi_logger *logger = NULL;
    te_string scene = TE_STRING_INIT;
    tapi_cv_region_stats stats;
    tapi_cv_rect green_rect = {
        TSAPI_CV_GREEN_X, TSAPI_CV_GREEN_Y, TSAPI_CV_GREEN_W, TSAPI_CV_GREEN_H,
    };
    tapi_cv_rect red_rect = {
        TSAPI_CV_RED_X, TSAPI_CV_RED_Y, TSAPI_CV_RED_W, TSAPI_CV_RED_H,
    };
    unsigned int i;
    const tapi_cv_rect *rects[3];
    static const char * const names[] = { "indicator", "red", "whole" };

    TEST_START;

    TEST_STEP("Open a session and generate the fixtures");
    CHECK_RC(tsapi_cv_session_init(&sess, "pco_cv_region"));
    if (!tapi_cv_available(sess.pco))
        TEST_SKIP("The agent has no OpenCV");
    rc = tsapi_cv_fixtures(&sess);
    if (rc != 0)
    {
        if (TE_RC_GET_ERROR(rc) == TE_EOPNOTSUPP)
            TEST_SKIP("The agent cannot generate fixtures (no python3+cv2)");
        CHECK_RC(rc);
    }

    tsapi_cv_fixture(&sess, "scene.png", &scene);

    TEST_STEP("The indicator is green");
    CHECK_RC(tapi_cv_region(sess.pco, scene.ptr, &green_rect, &green, &stats));
    tapi_cv_region_log(&stats);
    if (stats.in_range_fraction < 0.9)
        TEST_VERDICT("The indicator is only %.0f%% green, expected ~100%%",
                     stats.in_range_fraction * 100.0);

    TEST_STEP("The red block is not green");
    CHECK_RC(tapi_cv_region(sess.pco, scene.ptr, &red_rect, &green, &stats));
    tapi_cv_region_log(&stats);
    if (stats.in_range_fraction > 0.1)
        TEST_VERDICT("The red block is %.0f%% green, expected ~0%%",
                     stats.in_range_fraction * 100.0);
    /* Its mean colour is red-dominant (BGR). */
    if (stats.mean_r <= stats.mean_b || stats.mean_r <= stats.mean_g)
        TEST_VERDICT("The red block mean BGR is (%d,%d,%d), not red-dominant",
                     stats.mean_b, stats.mean_g, stats.mean_r);

    TEST_STEP("Chart the green share across regions");
    CHECK_RC(te_mi_logger_meas_create("tsf-cv", &logger));
    rects[0] = &green_rect;
    rects[1] = &red_rect;
    rects[2] = NULL; /* whole image */
    for (i = 0; i < TE_ARRAY_LEN(rects); i++)
    {
        CHECK_RC(tapi_cv_region(sess.pco, scene.ptr, rects[i], &green, &stats));
        RING("%s: %.1f%% green", names[i], stats.in_range_fraction * 100.0);

        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "region", TE_MI_MEAS_AGGR_SINGLE, i,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_PERCENTAGE,
                              "green", TE_MI_MEAS_AGGR_SINGLE,
                              stats.in_range_fraction * 100.0,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
    }

    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "green", "Green share by region (%)");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "green",
                               TE_MI_GRAPH_AXIS_X, "region");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "green", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_PERCENTAGE, "green");
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    te_string_free(&scene);
    tsapi_cv_session_fini(&sess);
    TEST_END;
}
