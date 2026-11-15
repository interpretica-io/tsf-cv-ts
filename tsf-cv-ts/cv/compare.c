/** @file
 * @brief CV Group
 *
 * The visual-regression primitive. Two copies of the scene are
 * identical (zero changed pixels, SSIM 1); the diff variant adds one
 * black rectangle, so the compare reports changed pixels and a bounding
 * box around that rectangle. A sweep of the tolerance plots how the
 * changed-pixel count falls as the per-channel tolerance rises.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "cv/compare"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_cv.h"
#include "tsapi_cv.h"

/* Does a value fall within [lo, hi]? */
static bool
near(int v, int lo, int hi)
{
    return v >= lo && v <= hi;
}

int
main(int argc, char **argv)
{
    tsapi_cv_session sess;
    te_mi_logger *logger = NULL;
    te_string scene = TE_STRING_INIT;
    te_string same = TE_STRING_INIT;
    te_string diff_img = TE_STRING_INIT;
    tapi_cv_diff diff;
    tapi_cv_compare_opts opts = {0};
    int tol;

    TEST_START;

    TEST_STEP("Open a session and generate the fixtures");
    CHECK_RC(tsapi_cv_session_init(&sess, "pco_cv_compare"));
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
    tsapi_cv_fixture(&sess, "scene_same.png", &same);
    tsapi_cv_fixture(&sess, "scene_diff.png", &diff_img);

    CHECK_RC(te_mi_logger_meas_create("tsf-cv", &logger));

    TEST_STEP("Identical images report no change and SSIM 1");
    CHECK_RC(tapi_cv_compare(sess.pco, scene.ptr, same.ptr, NULL, &diff));
    tapi_cv_diff_log(&diff);
    if (diff.diff_pixels != 0)
        TEST_VERDICT("Identical images reported %u changed pixels",
                     diff.diff_pixels);
    if (diff.ssim < 0.999)
        TEST_VERDICT("Identical images reported SSIM %.4f, expected 1",
                     diff.ssim);
    tapi_cv_diff_free(&diff);

    TEST_STEP("The diff variant is found, boxed around the added rectangle");
    opts.want_diff = true;
    CHECK_RC(tapi_cv_compare(sess.pco, scene.ptr, diff_img.ptr, &opts, &diff));
    tapi_cv_diff_log(&diff);
    if (diff.diff_pixels == 0)
        TEST_VERDICT("The diff variant reported no change");
    /* The change is a TSAPI_CV_DIFF_W x TSAPI_CV_DIFF_H rectangle whose
     * top-left is (TSAPI_CV_DIFF_X, TSAPI_CV_DIFF_Y); allow a few px of
     * slack for anti-aliasing at the edges. */
    if (!near(diff.bbox_x, TSAPI_CV_DIFF_X - 2, TSAPI_CV_DIFF_X + 2) ||
        !near(diff.bbox_y, TSAPI_CV_DIFF_Y - 2, TSAPI_CV_DIFF_Y + 2))
    {
        TEST_VERDICT("Changed region boxed at (%d,%d), expected near (%d,%d)",
                     diff.bbox_x, diff.bbox_y, TSAPI_CV_DIFF_X,
                     TSAPI_CV_DIFF_Y);
    }
    if (diff.diff_path == NULL)
        TEST_VERDICT("A diff heatmap was requested but none was returned");
    tapi_cv_diff_free(&diff);

    TEST_STEP("Chart changed-pixel count against the tolerance");
    for (tol = 0; tol <= 275; tol += 25)
    {
        tapi_cv_compare_opts o = {0};

        o.tolerance = tol;
        CHECK_RC(tapi_cv_compare(sess.pco, scene.ptr, diff_img.ptr, &o, &diff));
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "tolerance", TE_MI_MEAS_AGGR_SINGLE, tol,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "changed", TE_MI_MEAS_AGGR_SINGLE,
                              diff.diff_pixels, TE_MI_MEAS_MULTIPLIER_PLAIN);
        tapi_cv_diff_free(&diff);
    }

    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "tol", "Changed pixels vs. tolerance");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "tol",
                               TE_MI_GRAPH_AXIS_X, "tolerance");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "tol", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "changed");
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    te_string_free(&scene);
    te_string_free(&same);
    te_string_free(&diff_img);
    tsapi_cv_session_fini(&sess);
    TEST_END;
}
