/** @file
 * @brief CV Group
 *
 * The end-to-end pairing tsf-cv is meant for: one realistic application
 * screenshot (title bar, a green status indicator, an OK button, a line
 * of status text and a QR code), checked with every operation at once -
 * its size, the button located by template match, the indicator proven
 * green, the status text read by OCR and the QR decoded. This is the
 * assertion layer a screenshot from tsf-playwright/tsf-android or a
 * tsf-emu framebuffer would feed, exercised against a known image.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "cv/screenshot"

#include "te_config.h"
#include <ctype.h>
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

/* Case-insensitive substring test. */
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

static bool
near(int v, int lo, int hi)
{
    return v >= lo && v <= hi;
}

int
main(int argc, char **argv)
{
    tsapi_cv_session sess;
    te_string ui = TE_STRING_INIT;
    te_string ok_tpl = TE_STRING_INIT;
    tapi_cv_image_info info;
    tapi_cv_match button;
    tapi_cv_region_stats stats;
    tapi_cv_rect pill = {
        TSAPI_CV_UI_GREEN_X, TSAPI_CV_UI_GREEN_Y,
        TSAPI_CV_UI_GREEN_W, TSAPI_CV_UI_GREEN_H,
    };

    TEST_START;

    TEST_STEP("Open a session and generate the fixtures");
    CHECK_RC(tsapi_cv_session_init(&sess, "pco_cv_screenshot"));
    if (!tapi_cv_available(sess.pco))
        TEST_SKIP("The agent has no OpenCV");
    rc = tsapi_cv_fixtures(&sess);
    if (rc != 0)
    {
        if (TE_RC_GET_ERROR(rc) == TE_EOPNOTSUPP)
            TEST_SKIP("The agent cannot generate fixtures (no python3+cv2)");
        CHECK_RC(rc);
    }

    tsapi_cv_fixture(&sess, "ui.png", &ui);
    tsapi_cv_fixture(&sess, "ui_ok_tpl.png", &ok_tpl);

    TEST_STEP("The screenshot is the size it was rendered");
    CHECK_RC(tapi_cv_info(sess.pco, ui.ptr, &info));
    RING("ui.png: %dx%d, %d ch, %d-bit, %s", info.width, info.height,
         info.channels, info.depth, info.format);
    if (info.width != TSAPI_CV_UI_W || info.height != TSAPI_CV_UI_H)
    {
        tapi_cv_image_info_free(&info);
        TEST_VERDICT("ui.png is %dx%d, expected %dx%d", info.width, info.height,
                     TSAPI_CV_UI_W, TSAPI_CV_UI_H);
    }
    tapi_cv_image_info_free(&info);

    TEST_STEP("The OK button is located by template match");
    if (!tapi_cv_find(sess.pco, ui.ptr, ok_tpl.ptr, 0.9, &button))
        TEST_VERDICT("The OK button was not found in the screenshot");
    RING("OK button: score %.4f at (%d,%d)", button.score, button.x, button.y);
    if (!near(button.x, TSAPI_CV_UI_OK_X - 2, TSAPI_CV_UI_OK_X + 2) ||
        !near(button.y, TSAPI_CV_UI_OK_Y - 2, TSAPI_CV_UI_OK_Y + 2))
    {
        TEST_VERDICT("OK button matched at (%d,%d), expected near (%d,%d)",
                     button.x, button.y, TSAPI_CV_UI_OK_X, TSAPI_CV_UI_OK_Y);
    }

    TEST_STEP("The status indicator is green");
    CHECK_RC(tapi_cv_region(sess.pco, ui.ptr, &pill, &green, &stats));
    tapi_cv_region_log(&stats);
    if (stats.in_range_fraction < 0.9)
        TEST_VERDICT("The indicator is only %.0f%% green, expected ~100%%",
                     stats.in_range_fraction * 100.0);

    TEST_STEP("The status text is read by OCR when the backend is present");
    if (!tapi_cv_supports(sess.pco, TAPI_CV_FEAT_OCR))
    {
        RING("No OCR backend on the agent; the text check is skipped");
    }
    else
    {
        tapi_cv_rect text_rect = {
            TSAPI_CV_UI_TEXT_X, TSAPI_CV_UI_TEXT_Y,
            TSAPI_CV_UI_TEXT_W, TSAPI_CV_UI_TEXT_H,
        };
        tapi_cv_text ocr;

        rc = tapi_cv_ocr(sess.pco, ui.ptr, &text_rect, NULL, &ocr);
        if (rc != 0 && TE_RC_GET_ERROR(rc) == TE_EOPNOTSUPP)
        {
            RING("No OCR language data on the agent; the text check is skipped");
        }
        else
        {
            CHECK_RC(rc);
            tapi_cv_text_log(&ocr);
            if (!contains_ci(ocr.text, "SYSTEM"))
            {
                tapi_cv_text_free(&ocr);
                TEST_VERDICT("OCR did not read \"SYSTEM\" from the status line");
            }
            tapi_cv_text_free(&ocr);
        }
    }

    TEST_STEP("The QR code is decoded when the backend is present");
    if (!tapi_cv_supports(sess.pco, TAPI_CV_FEAT_QR))
    {
        RING("No QR backend on the agent; the QR check is skipped");
    }
    else
    {
        tapi_cv_codes codes;
        const tapi_cv_code *c;
        bool found = false;

        CHECK_RC(tapi_cv_decode(sess.pco, ui.ptr, TAPI_CV_QR, &codes));
        tapi_cv_codes_log(&codes);
        TE_VEC_FOREACH(&codes.codes, c)
        {
            if (c->data != NULL && strcmp(c->data, TSAPI_CV_UI_QR_TEXT) == 0)
                found = true;
        }
        if (!found)
        {
            tapi_cv_codes_free(&codes);
            TEST_VERDICT("The screenshot QR \"%s\" was not decoded",
                         TSAPI_CV_UI_QR_TEXT);
        }
        tapi_cv_codes_free(&codes);
    }

    TEST_SUCCESS;

cleanup:
    te_string_free(&ui);
    te_string_free(&ok_tpl);
    tsapi_cv_session_fini(&sess);
    TEST_END;
}
