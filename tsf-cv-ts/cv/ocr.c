/** @file
 * @brief CV Group
 *
 * Reading text back from an image. The generator renders a known
 * string; OCR recovers it. Skipped when the agent has no tesseract.
 * The per-word confidence is charted.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "cv/ocr"

#include "te_config.h"
#include <ctype.h>
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_cv.h"
#include "tsapi_cv.h"

/* Case-insensitive "does haystack contain needle?". */
static bool
contains_ci(const char *haystack, const char *needle)
{
    size_t nlen = strlen(needle);
    const char *p;

    if (haystack == NULL)
        return false;
    for (p = haystack; *p != '\0'; p++)
    {
        size_t i;

        for (i = 0; i < nlen; i++)
        {
            if (toupper((unsigned char)p[i]) != toupper((unsigned char)needle[i]))
                break;
        }
        if (i == nlen)
            return true;
    }
    return false;
}

int
main(int argc, char **argv)
{
    tsapi_cv_session sess;
    te_mi_logger *logger = NULL;
    te_string text_img = TE_STRING_INIT;
    tapi_cv_text ocr_result;
    const tapi_cv_word *w;
    unsigned int i = 0;

    TEST_START;

    TEST_STEP("Open a session and generate the fixtures");
    CHECK_RC(tsapi_cv_session_init(&sess, "pco_cv_ocr"));
    if (!tapi_cv_available(sess.pco))
        TEST_SKIP("The agent has no OpenCV");
    if (!tapi_cv_supports(sess.pco, TAPI_CV_FEAT_OCR))
        TEST_SKIP("The agent has no OCR backend (libtesseract)");
    rc = tsapi_cv_fixtures(&sess);
    if (rc != 0)
    {
        if (TE_RC_GET_ERROR(rc) == TE_EOPNOTSUPP)
            TEST_SKIP("The agent cannot generate fixtures (no python3+cv2)");
        CHECK_RC(rc);
    }

    tsapi_cv_fixture(&sess, "text.png", &text_img);

    TEST_STEP("The rendered text is recognised");
    rc = tapi_cv_ocr(sess.pco, text_img.ptr, NULL, NULL, &ocr_result);
    if (rc != 0 && TE_RC_GET_ERROR(rc) == TE_EOPNOTSUPP)
        TEST_SKIP("The agent has no OCR language data");
    CHECK_RC(rc);
    tapi_cv_text_log(&ocr_result);

    if (!contains_ci(ocr_result.text, "HELLO"))
        TEST_VERDICT("OCR did not recover \"HELLO\" (got \"%s\")",
                     ocr_result.text != NULL ? ocr_result.text : "");
    if (!contains_ci(ocr_result.text, "2026"))
        TEST_VERDICT("OCR did not recover \"2026\" (got \"%s\")",
                     ocr_result.text != NULL ? ocr_result.text : "");

    TEST_STEP("Chart the per-word confidence");
    CHECK_RC(te_mi_logger_meas_create("tsf-cv", &logger));
    TE_VEC_FOREACH(&ocr_result.words, w)
    {
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "word", TE_MI_MEAS_AGGR_SINGLE, i++,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_PERCENTAGE,
                              "confidence", TE_MI_MEAS_AGGR_SINGLE, w->conf,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
    }
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "conf", "OCR confidence per word (%)");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "conf",
                               TE_MI_GRAPH_AXIS_X, "word");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "conf", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_PERCENTAGE, "confidence");
    te_mi_logger_destroy(logger);
    logger = NULL;

    tapi_cv_text_free(&ocr_result);

    TEST_SUCCESS;

cleanup:
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    te_string_free(&text_img);
    tsapi_cv_session_fini(&sess);
    TEST_END;
}
