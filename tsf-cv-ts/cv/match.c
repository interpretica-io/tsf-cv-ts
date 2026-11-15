/** @file
 * @brief CV Group
 *
 * Finding a template inside an image. The red rectangle cropped out of
 * the scene is located exactly where it was cropped from; a template
 * that is not in the scene (deterministic noise) is not found. The best
 * score for the present template is charted for each method family.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "cv/match"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_cv.h"
#include "tsapi_cv.h"

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
    te_string tpl_red = TE_STRING_INIT;
    te_string tpl_absent = TE_STRING_INIT;
    tapi_cv_match best;
    unsigned int i;
    static const tapi_cv_match_method families[] = {
        TAPI_CV_SQDIFF, TAPI_CV_CCORR, TAPI_CV_CCOEFF,
    };

    TEST_START;

    TEST_STEP("Open a session and generate the fixtures");
    CHECK_RC(tsapi_cv_session_init(&sess, "pco_cv_match"));
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
    tsapi_cv_fixture(&sess, "tpl_red.png", &tpl_red);
    tsapi_cv_fixture(&sess, "tpl_absent.png", &tpl_absent);

    TEST_STEP("The red template is found where it was cropped from");
    if (!tapi_cv_find(sess.pco, scene.ptr, tpl_red.ptr, 0.9, &best))
        TEST_VERDICT("The red template was not found in the scene");
    RING("red template: score %.4f at (%d,%d) %dx%d", best.score, best.x,
         best.y, best.w, best.h);
    if (!near(best.x, TSAPI_CV_RED_X - 2, TSAPI_CV_RED_X + 2) ||
        !near(best.y, TSAPI_CV_RED_Y - 2, TSAPI_CV_RED_Y + 2))
    {
        TEST_VERDICT("Red template matched at (%d,%d), expected near (%d,%d)",
                     best.x, best.y, TSAPI_CV_RED_X, TSAPI_CV_RED_Y);
    }

    TEST_STEP("A template that is not present is not found");
    if (tapi_cv_find(sess.pco, scene.ptr, tpl_absent.ptr, 0.9, NULL))
        TEST_VERDICT("An absent template was reported as found");

    TEST_STEP("The same template is found at both places it appears");
    {
        te_string multi_img = TE_STRING_INIT;
        tapi_cv_match_opts opts = {0};
        tapi_cv_matches matches;
        const tapi_cv_match *m;
        bool at_first = false;
        bool at_second = false;

        tsapi_cv_fixture(&sess, "scene_multi.png", &multi_img);
        /*
         * SQDIFF, not CCOEFF/CCORR: the template is a solid red block
         * with zero intra-template variance, and the normalised
         * correlation methods are degenerate on a zero-variance template
         * (they divide by its standard deviation), scoring 1.0 across the
         * flat white background and reporting matches in the corner
         * rather than on the blocks. SQDIFF is difference-based and has
         * no such singularity, so it is the right method for a uniform
         * template.
         */
        opts.method = TAPI_CV_SQDIFF;
        opts.threshold = 0.9;
        opts.max_matches = 4;
        CHECK_RC(tapi_cv_match_all(sess.pco, multi_img.ptr, tpl_red.ptr, &opts,
                               &matches));
        tapi_cv_matches_log(&matches);
        TE_VEC_FOREACH(&matches.matches, m)
        {
            if (near(m->x, TSAPI_CV_RED_X - 2, TSAPI_CV_RED_X + 2) &&
                near(m->y, TSAPI_CV_RED_Y - 2, TSAPI_CV_RED_Y + 2))
                at_first = true;
            if (near(m->x, TSAPI_CV_RED2_X - 2, TSAPI_CV_RED2_X + 2) &&
                near(m->y, TSAPI_CV_RED2_Y - 2, TSAPI_CV_RED2_Y + 2))
                at_second = true;
        }
        if (!at_first || !at_second)
        {
            TEST_VERDICT("The template was not found at both (%d,%d) and "
                         "(%d,%d): %u match(es)", TSAPI_CV_RED_X, TSAPI_CV_RED_Y,
                         TSAPI_CV_RED2_X, TSAPI_CV_RED2_Y,
                         (unsigned)te_vec_size(&matches.matches));
        }
        tapi_cv_matches_free(&matches);
        te_string_free(&multi_img);
    }

    TEST_STEP("Chart the best red-template score per method family");
    CHECK_RC(te_mi_logger_meas_create("tsf-cv", &logger));
    for (i = 0; i < TE_ARRAY_LEN(families); i++)
    {
        tapi_cv_match_opts opts = {0};
        tapi_cv_matches matches;

        opts.method = families[i];
        opts.threshold = 0.0;
        opts.max_matches = 1;
        CHECK_RC(tapi_cv_match_all(sess.pco, scene.ptr, tpl_red.ptr, &opts,
                               &matches));
        RING("%s: best %.4f", tapi_cv_method2str(families[i]), matches.best);

        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "method", TE_MI_MEAS_AGGR_SINGLE, i,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "score", TE_MI_MEAS_AGGR_SINGLE, matches.best,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        tapi_cv_matches_free(&matches);
    }

    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "score", "Best match score by method family");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "score",
                               TE_MI_GRAPH_AXIS_X, "method");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "score", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "score");
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    te_string_free(&scene);
    te_string_free(&tpl_red);
    te_string_free(&tpl_absent);
    tsapi_cv_session_fini(&sess);
    TEST_END;
}
