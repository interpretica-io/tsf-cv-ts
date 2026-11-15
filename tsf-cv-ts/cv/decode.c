/** @file
 * @brief CV Group
 *
 * Decoding a QR symbol. The generator encodes a known payload (when the
 * agent's OpenCV can); tapi_cv_decode reads it back. Skipped when the
 * agent cannot encode the fixture. The number of symbols found is
 * charted.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "cv/decode"

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
    te_mi_logger *logger = NULL;
    te_string qr = TE_STRING_INIT;
    tapi_cv_image_info info;
    tapi_cv_codes codes;
    const tapi_cv_code *c;
    bool found = false;

    TEST_START;

    TEST_STEP("Open a session and generate the fixtures");
    CHECK_RC(tsapi_cv_session_init(&sess, "pco_cv_decode"));
    if (!tapi_cv_available(sess.pco))
        TEST_SKIP("The agent has no OpenCV");
    if (!tapi_cv_supports(sess.pco, TAPI_CV_FEAT_QR))
        TEST_SKIP("The agent has no QR backend");
    rc = tsapi_cv_fixtures(&sess);
    if (rc != 0)
    {
        if (TE_RC_GET_ERROR(rc) == TE_EOPNOTSUPP)
            TEST_SKIP("The agent cannot generate fixtures (no python3+cv2)");
        CHECK_RC(rc);
    }

    tsapi_cv_fixture(&sess, "qr.png", &qr);

    /* The generator writes qr.png only when its OpenCV can encode QR. */
    if (tapi_cv_info(sess.pco, qr.ptr, &info) != 0)
        TEST_SKIP("The agent's OpenCV could not encode the QR fixture");
    tapi_cv_image_info_free(&info);

    TEST_STEP("The known QR payload is decoded");
    CHECK_RC(tapi_cv_decode(sess.pco, qr.ptr, TAPI_CV_QR, &codes));
    tapi_cv_codes_log(&codes);

    TE_VEC_FOREACH(&codes.codes, c)
    {
        if (c->data != NULL && strcmp(c->data, TSAPI_CV_QR_TEXT) == 0)
            found = true;
    }
    if (!found)
        TEST_VERDICT("The QR payload \"%s\" was not decoded", TSAPI_CV_QR_TEXT);

    TEST_STEP("Chart the number of symbols found");
    CHECK_RC(te_mi_logger_meas_create("tsf-cv", &logger));
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "symbols", TE_MI_MEAS_AGGR_SINGLE,
                          te_vec_size(&codes.codes),
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "sym", "Symbols decoded");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "sym",
                               TE_MI_GRAPH_AXIS_X, TE_MI_GRAPH_AUTO_SEQNO);
    te_mi_logger_destroy(logger);
    logger = NULL;

    tapi_cv_codes_free(&codes);

    TEST_SUCCESS;

cleanup:
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    te_string_free(&qr);
    tsapi_cv_session_fini(&sess);
    TEST_END;
}
