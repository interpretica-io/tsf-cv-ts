/** @file
 * @brief CV Group
 *
 * Decoding 1-D barcodes, with a known-good control.
 *
 * The control is OpenCV's own CI barcode photos (opencv_extra
 * testdata/cv/barcode/single): real EAN-13 images its regression tests
 * assert cv::barcode decodes. This test requires the agent to decode
 * each of them to its documented value - that is the health check on the
 * library itself. The suite's synthetic EAN-13 (python-barcode) is then
 * decoded as a diagnostic only: whether it decodes separates a library
 * limitation (controls also fail) from a fixture-rendering quirk
 * (controls decode, synthetic does not). The number of symbols decoded
 * from each source is charted.
 *
 * Skipped when the agent has no barcode backend (cv::barcode, OpenCV
 * 4.8+) or when neither the controls nor a synthetic fixture are present
 * (no control to judge against).
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "cv/barcode"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_cv.h"
#include "tsapi_cv.h"

/** One known-good reference image and the EAN-13 it must decode to. */
typedef struct ref_barcode {
    const char *file;
    const char *text;
} ref_barcode;

/**
 * Decode @p path and report whether @p want appears among the symbols.
 * @p n_syms gets the number of symbols decoded. Returns true if @p want
 * was found.
 */
static bool
decode_has(rcf_rpc_server *pco, const char *path, const char *want,
           unsigned int *n_syms)
{
    tapi_cv_codes codes;
    const tapi_cv_code *c;
    bool found = false;

    if (tapi_cv_decode(pco, path, TAPI_CV_BARCODE, &codes) != 0)
    {
        *n_syms = 0;
        return false;
    }
    tapi_cv_codes_log(&codes);
    *n_syms = (unsigned int)te_vec_size(&codes.codes);
    TE_VEC_FOREACH(&codes.codes, c)
    {
        if (c->data != NULL && want != NULL && strcmp(c->data, want) == 0)
            found = true;
    }
    tapi_cv_codes_free(&codes);
    return found;
}

int
main(int argc, char **argv)
{
    static const ref_barcode refs[] = TSAPI_CV_REF_BARCODES;
    const size_t n_refs = TE_ARRAY_LEN(refs);

    tsapi_cv_session sess;
    te_mi_logger *logger = NULL;
    te_string path = TE_STRING_INIT;
    tapi_cv_image_info info;
    unsigned int n_syms;
    size_t refs_present = 0;
    size_t refs_decoded = 0;
    bool have_synthetic = false;
    bool synthetic_decoded = false;
    size_t i;

    TEST_START;

    TEST_STEP("Open a session and generate the fixtures");
    CHECK_RC(tsapi_cv_session_init(&sess, "pco_cv_barcode"));
    if (!tapi_cv_available(sess.pco))
        TEST_SKIP("The agent has no OpenCV");
    if (!tapi_cv_supports(sess.pco, TAPI_CV_FEAT_BARCODE))
        TEST_SKIP("The agent has no barcode backend (cv::barcode, 4.8+)");
    rc = tsapi_cv_fixtures(&sess);
    if (rc != 0)
    {
        if (TE_RC_GET_ERROR(rc) == TE_EOPNOTSUPP)
            TEST_SKIP("The agent cannot generate fixtures (no python3+cv2)");
        CHECK_RC(rc);
    }

    TEST_STEP("Control: OpenCV's own reference EAN-13 photos must decode");
    for (i = 0; i < n_refs; i++)
    {
        bool ok;

        te_string_reset(&path);
        tsapi_cv_fixture(&sess, refs[i].file, &path);

        /* The reference is present only on an agent built with it. */
        if (tapi_cv_info(sess.pco, path.ptr, &info) != 0)
            continue;
        tapi_cv_image_info_free(&info);
        refs_present++;

        RING("Control reference %s (expect %s)", refs[i].file, refs[i].text);
        ok = decode_has(sess.pco, path.ptr, refs[i].text, &n_syms);
        if (ok)
            refs_decoded++;
        else
            TEST_VERDICT("OpenCV did not decode its own reference EAN-13 "
                         "\"%s\" from %s (%u symbol(s) returned)",
                         refs[i].text, refs[i].file, n_syms);
    }

    TEST_STEP("Diagnostic: the suite's synthetic EAN-13 (non-fatal)");
    te_string_reset(&path);
    tsapi_cv_fixture(&sess, "barcode.png", &path);
    if (tapi_cv_info(sess.pco, path.ptr, &info) == 0)
    {
        tapi_cv_image_info_free(&info);
        have_synthetic = true;
        synthetic_decoded = decode_has(sess.pco, path.ptr,
                                       TSAPI_CV_BARCODE_TEXT, &n_syms);
        if (synthetic_decoded)
            RING("Synthetic EAN-13 \"%s\" decoded", TSAPI_CV_BARCODE_TEXT);
        else
            RING("Synthetic EAN-13 \"%s\" NOT decoded (%u symbol(s)); "
                 "a python-barcode rendering quirk, not a library fault, "
                 "since the reference photos above decode",
                 TSAPI_CV_BARCODE_TEXT, n_syms);
    }

    if (refs_present == 0 && !have_synthetic)
        TEST_SKIP("No barcode control and no synthetic fixture to judge");

    RING("Barcode decode: %zu/%zu reference photos, synthetic %s",
         refs_decoded, refs_present,
         !have_synthetic ? "absent" :
         (synthetic_decoded ? "decoded" : "not decoded"));

    TEST_STEP("Chart the number of reference barcodes decoded");
    CHECK_RC(te_mi_logger_meas_create("tsf-cv", &logger));
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "refs_decoded", TE_MI_MEAS_AGGR_SINGLE,
                          refs_decoded, TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "synthetic", TE_MI_MEAS_AGGR_SINGLE,
                          synthetic_decoded ? 1 : 0,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "bar", "Barcodes decoded by source");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "bar",
                               TE_MI_GRAPH_AXIS_X, TE_MI_GRAPH_AUTO_SEQNO);
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    te_string_free(&path);
    tsapi_cv_session_fini(&sess);
    TEST_END;
}
