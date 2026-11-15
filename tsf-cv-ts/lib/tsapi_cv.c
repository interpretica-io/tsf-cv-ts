/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Suite helpers
 *
 * @author Maxim Menshikov <maxim.menshikov@interpretica.io>
 */

#define TE_LGR_USER "TSAPI CV"

#include "te_config.h"

#include <string.h>

#include "logger_api.h"
#include "te_alloc.h"
#include "te_string.h"
#include "tapi_file.h"
#include "tapi_cfg_base.h"
#include "tapi_rpc_stdio.h"

#include "tsapi_cv.h"

/*
 * The fixture generator: cv2 + numpy, standard across the suite. It
 * draws the known scene (see the geometry macros in tsapi_cv.h), a
 * zero-diff copy, a one-rectangle diff variant, the match template and
 * a template that is absent, a big-text image for OCR, and - when the
 * agent's OpenCV can encode one - a QR image. It takes the output
 * directory as its one argument and creates it. Keep the drawing here
 * in step with the macros the tests assert against.
 */
static const char cvgen_py[] =
    "import sys, os\n"
    "import numpy as np\n"
    "import cv2\n"
    "out = sys.argv[1]\n"
    "os.makedirs(out, exist_ok=True)\n"
    "W, H = 320, 240\n"
    "img = np.full((H, W, 3), 255, np.uint8)\n"
    "cv2.rectangle(img, (40, 40), (120, 100), (0, 0, 255), -1)\n"
    "cv2.rectangle(img, (200, 60), (260, 120), (0, 255, 0), -1)\n"
    "cv2.circle(img, (160, 180), 30, (255, 0, 0), -1)\n"
    "cv2.putText(img, 'HELLO CV 2026', (30, 230),\n"
    "            cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 0, 0), 2, cv2.LINE_AA)\n"
    "cv2.imwrite(os.path.join(out, 'scene.png'), img)\n"
    "cv2.imwrite(os.path.join(out, 'scene_same.png'), img.copy())\n"
    "d = img.copy()\n"
    "cv2.rectangle(d, (150, 150), (180, 170), (0, 0, 0), -1)\n"
    "cv2.imwrite(os.path.join(out, 'scene_diff.png'), d)\n"
    "cv2.imwrite(os.path.join(out, 'tpl_red.png'), img[40:100, 40:120].copy())\n"
    "rng = np.random.RandomState(42)\n"
    "noise = rng.randint(0, 256, (30, 30, 3)).astype(np.uint8)\n"
    "cv2.imwrite(os.path.join(out, 'tpl_absent.png'), noise)\n"
    "t = np.full((120, 600, 3), 255, np.uint8)\n"
    "cv2.putText(t, 'HELLO CV 2026', (20, 80),\n"
    "            cv2.FONT_HERSHEY_SIMPLEX, 2.0, (0, 0, 0), 4, cv2.LINE_AA)\n"
    "cv2.imwrite(os.path.join(out, 'text.png'), t)\n"
    "try:\n"
    "    enc = cv2.QRCodeEncoder_create()\n"
    "    qr = enc.encode('tsf-cv:ok')\n"
    "    qr = cv2.copyMakeBorder(qr, 4, 4, 4, 4,\n"
    "                            cv2.BORDER_CONSTANT, value=255)\n"
    "    qr = cv2.resize(qr, None, fx=12, fy=12,\n"
    "                    interpolation=cv2.INTER_NEAREST)\n"
    "    qr = cv2.cvtColor(qr, cv2.COLOR_GRAY2BGR)\n"
    "    cv2.imwrite(os.path.join(out, 'qr.png'), qr)\n"
    "except Exception as e:\n"
    "    sys.stderr.write('no QR encoder: %s\\n' % e)\n"
    "print(out)\n";

/* See description in tsapi_cv.h */
te_errno
tsapi_cv_session_init(tsapi_cv_session *session, const char *name)
{
    te_errno rc;

    memset(session, 0, sizeof(*session));
    session->ta = TSAPI_CV_TA;

    rc = rcf_rpc_server_create(session->ta, name, &session->pco);
    if (rc != 0)
    {
        ERROR("Cannot create the RPC server '%s' on %s: %r", name,
              session->ta, rc);
        return rc;
    }

    return 0;
}

/* See description in tsapi_cv.h */
te_errno
tsapi_cv_fixtures(tsapi_cv_session *session)
{
    char *tmp = NULL;
    te_string script = TE_STRING_INIT;
    te_string dir = TE_STRING_INIT;
    te_string cmd = TE_STRING_INIT;
    rpc_wait_status st;
    te_errno rc = 0;

    tmp = tapi_cfg_base_get_ta_dir(session->ta, TAPI_CFG_BASE_TA_DIR_TMP);
    if (tmp == NULL)
        return TE_RC(TE_TAPI, TE_EFAIL);

    tapi_file_make_custom_pathname(&script, tmp, "-cvgen.py");
    tapi_file_make_custom_pathname(&dir, tmp, "-cvfix");

    rc = tapi_file_create_ta(session->ta, script.ptr, "%s", cvgen_py);
    if (rc != 0)
    {
        ERROR("Cannot write the fixture generator onto %s: %r", session->ta,
              rc);
        goto out;
    }

    te_string_append(&cmd, "python3 %s %s", script.ptr, dir.ptr);
    RPC_AWAIT_IUT_ERROR(session->pco);
    st = rpc_system(session->pco, cmd.ptr);
    if (st.flag != RPC_WAIT_STATUS_EXITED || st.value != 0)
    {
        /* No usable python3+cv2 on the agent: the caller skips. */
        WARN("Fixture generator did not succeed (flag %d, value %u): "
             "the agent has no usable python3+cv2", st.flag, st.value);
        rc = TE_RC(TE_TAPI, TE_EOPNOTSUPP);
        goto out;
    }

    session->dir = TE_STRDUP(dir.ptr);
    RING("Fixtures generated in %s on %s", session->dir, session->ta);

out:
    free(tmp);
    te_string_free(&script);
    te_string_free(&dir);
    te_string_free(&cmd);
    return rc;
}

/* See description in tsapi_cv.h */
const char *
tsapi_cv_fixture(const tsapi_cv_session *session, const char *name,
                 te_string *path)
{
    te_string_append(path, "%s/%s", session->dir != NULL ? session->dir : ".",
                     name);
    return path->ptr;
}

/* See description in tsapi_cv.h */
void
tsapi_cv_session_fini(tsapi_cv_session *session)
{
    free(session->dir);
    session->dir = NULL;

    if (session->pco != NULL)
    {
        rcf_rpc_server_destroy(session->pco);
        session->pco = NULL;
    }
}
