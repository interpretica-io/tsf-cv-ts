/** @file
 * @brief CV Group
 *
 * Decoding QR symbols in an image: a known single payload and a second
 * image carrying two QR symbols, both read back by tapi_cv_decode. QR
 * needs an OpenCV built with QUIRC (tapi_cv_supports reports it). 1-D
 * barcode decoding is exercised by its own test (barcode.c). The number
 * of symbols decoded is charted.
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

/* Is @p want among the decoded payloads? */
static bool
has_payload(const tapi_cv_codes *codes, const char *want)
{
    const tapi_cv_code *c;

    TE_VEC_FOREACH(&codes->codes, c)
    {
        if (c->data != NULL && strcmp(c->data, want) == 0)
            return true;
    }
    return false;
}

int
main(int argc, char **argv)
{
    tsapi_cv_session sess;
    te_mi_logger *logger = NULL;
    te_string qr = TE_STRING_INIT;
    te_string qr2 = TE_STRING_INIT;
    tapi_cv_image_info info;
    tapi_cv_codes codes;
    unsigned int total = 0;

    TEST_START;

    TEST_STEP("Open a session and generate the fixtures");
    CHECK_RC(tsapi_cv_session_init(&sess, "pco_cv_decode"));
    if (!tapi_cv_available(sess.pco))
        TEST_SKIP("The agent has no OpenCV");
    if (!tapi_cv_supports(sess.pco, TAPI_CV_FEAT_QR))
        TEST_SKIP("The agent has no QR backend (OpenCV built without QUIRC)");
    rc = tsapi_cv_fixtures(&sess);
    if (rc != 0)
    {
        if (TE_RC_GET_ERROR(rc) == TE_EOPNOTSUPP)
            TEST_SKIP("The agent cannot generate fixtures (no python3+cv2)");
        CHECK_RC(rc);
    }

    tsapi_cv_fixture(&sess, "qr.png", &qr);
    tsapi_cv_fixture(&sess, "qr2.png", &qr2);

    /* qr.png exists only when the agent's OpenCV can encode QR. */
    if (tapi_cv_info(sess.pco, qr.ptr, &info) != 0)
        TEST_SKIP("The agent's OpenCV could not encode the QR fixtures");
    tapi_cv_image_info_free(&info);

    TEST_STEP("A single known QR payload is decoded");
    CHECK_RC(tapi_cv_decode(sess.pco, qr.ptr, TAPI_CV_QR, &codes));
    tapi_cv_codes_log(&codes);
    if (!has_payload(&codes, TSAPI_CV_QR_TEXT))
        TEST_VERDICT("The QR payload \"%s\" was not decoded", TSAPI_CV_QR_TEXT);
    total += te_vec_size(&codes.codes);
    tapi_cv_codes_free(&codes);

    TEST_STEP("Both QR symbols in a two-code image are decoded");
    CHECK_RC(tapi_cv_decode(sess.pco, qr2.ptr, TAPI_CV_QR, &codes));
    tapi_cv_codes_log(&codes);
    if (!has_payload(&codes, TSAPI_CV_QR1_TEXT) ||
        !has_payload(&codes, TSAPI_CV_QR2_TEXT))
    {
        TEST_VERDICT("Both QR payloads \"%s\" and \"%s\" were not decoded "
                     "(got %u symbols)", TSAPI_CV_QR1_TEXT, TSAPI_CV_QR2_TEXT,
                     (unsigned)te_vec_size(&codes.codes));
    }
    total += te_vec_size(&codes.codes);
    tapi_cv_codes_free(&codes);

    TEST_STEP("Chart the number of symbols decoded");
    CHECK_RC(te_mi_logger_meas_create("tsf-cv", &logger));
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "symbols", TE_MI_MEAS_AGGR_SINGLE, total,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "sym", "Symbols decoded");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "sym",
                               TE_MI_GRAPH_AXIS_X, TE_MI_GRAPH_AUTO_SEQNO);
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    te_string_free(&qr);
    te_string_free(&qr2);
    tsapi_cv_session_fini(&sess);
    TEST_END;
}
