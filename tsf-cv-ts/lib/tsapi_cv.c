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
 * draws every image the tests look at, all with geometry and contents
 * fixed by the macros in tsapi_cv.h, which the tests assert against:
 *
 *   scene.png / scene_same.png / scene_diff.png  the visual-regression
 *                                                scene and its variants
 *   tpl_red.png / tpl_absent.png                 template-match inputs
 *   scene_multi.png                              the red block drawn twice
 *   text.png                                     a big string for OCR
 *   scene.jpg / scene.bmp / scene.tiff / gray.png / deep16.png
 *                                                format and depth variants
 *   qr.png / qr2.png                             one and two QR symbols
 *   barcode.png                                  an EAN-13 1-D barcode
 *   ui.png / ui_ok_tpl.png                       a realistic app screenshot
 *                                                and the OK-button template
 *
 * It takes the output directory as its one argument and creates it. The
 * QR, multi-QR, barcode and screenshot-QR fixtures are each wrapped so a
 * backend the agent lacks (no QR encoder, no python-barcode) only drops
 * that one image; the test that needs it then skips.
 */
static const char cvgen_py[] =
    "import sys, os\n"
    "import numpy as np\n"
    "import cv2\n"
    "out = sys.argv[1]\n"
    "os.makedirs(out, exist_ok=True)\n"
    "def P(n): return os.path.join(out, n)\n"
    "W, H = 320, 240\n"
    "img = np.full((H, W, 3), 255, np.uint8)\n"
    "cv2.rectangle(img, (40, 40), (120, 100), (0, 0, 255), -1)\n"
    "cv2.rectangle(img, (200, 60), (260, 120), (0, 255, 0), -1)\n"
    "cv2.circle(img, (160, 180), 30, (255, 0, 0), -1)\n"
    "cv2.putText(img, 'HELLO CV 2026', (30, 230),\n"
    "            cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 0, 0), 2, cv2.LINE_AA)\n"
    "cv2.imwrite(P('scene.png'), img)\n"
    "cv2.imwrite(P('scene_same.png'), img.copy())\n"
    "d = img.copy()\n"
    "cv2.rectangle(d, (150, 150), (180, 170), (0, 0, 0), -1)\n"
    "cv2.imwrite(P('scene_diff.png'), d)\n"
    "cv2.imwrite(P('tpl_red.png'), img[40:100, 40:120].copy())\n"
    "rng = np.random.RandomState(42)\n"
    "noise = rng.randint(0, 256, (30, 30, 3)).astype(np.uint8)\n"
    "cv2.imwrite(P('tpl_absent.png'), noise)\n"
    "multi = np.full((H, W, 3), 255, np.uint8)\n"
    "cv2.rectangle(multi, (40, 40), (120, 100), (0, 0, 255), -1)\n"
    "cv2.rectangle(multi, (220, 150), (300, 210), (0, 0, 255), -1)\n"
    "cv2.imwrite(P('scene_multi.png'), multi)\n"
    "t = np.full((120, 600, 3), 255, np.uint8)\n"
    "cv2.putText(t, 'HELLO CV 2026', (20, 80),\n"
    "            cv2.FONT_HERSHEY_SIMPLEX, 2.0, (0, 0, 0), 4, cv2.LINE_AA)\n"
    "cv2.imwrite(P('text.png'), t)\n"
    "cv2.imwrite(P('scene.jpg'), img, [int(cv2.IMWRITE_JPEG_QUALITY), 95])\n"
    "cv2.imwrite(P('scene.bmp'), img)\n"
    "cv2.imwrite(P('scene.tiff'), img)\n"
    "cv2.imwrite(P('gray.png'), cv2.cvtColor(img, cv2.COLOR_BGR2GRAY))\n"
    "cv2.imwrite(P('deep16.png'), (img.astype(np.uint16) * 257))\n"
    "def mk_qr(s, scale=8):\n"
    "    enc = cv2.QRCodeEncoder_create()\n"
    "    m = enc.encode(s)\n"
    "    m = cv2.copyMakeBorder(m, 4, 4, 4, 4, cv2.BORDER_CONSTANT, value=255)\n"
    "    m = cv2.resize(m, None, fx=scale, fy=scale,\n"
    "                   interpolation=cv2.INTER_NEAREST)\n"
    "    return cv2.cvtColor(m, cv2.COLOR_GRAY2BGR)\n"
    "try:\n"
    "    cv2.imwrite(P('qr.png'), mk_qr('tsf-cv:ok', 12))\n"
    "except Exception as e:\n"
    "    sys.stderr.write('no QR encoder: %s\\n' % e)\n"
    "try:\n"
    "    a = mk_qr('tsf-cv:one'); b = mk_qr('tsf-cv:two')\n"
    "    hh = max(a.shape[0], b.shape[0])\n"
    "    canvas = np.full((hh, a.shape[1] + b.shape[1] + 40, 3), 255, np.uint8)\n"
    "    canvas[0:a.shape[0], 0:a.shape[1]] = a\n"
    "    xo = a.shape[1] + 40\n"
    "    canvas[0:b.shape[0], xo:xo + b.shape[1]] = b\n"
    "    cv2.imwrite(P('qr2.png'), canvas)\n"
    "except Exception as e:\n"
    "    sys.stderr.write('no multi QR: %s\\n' % e)\n"
    "try:\n"
    "    import barcode\n"
    "    from barcode.writer import ImageWriter\n"
    "    bopts = {'write_text': False, 'module_width': 0.6,\n"
    "             'module_height': 25.0, 'quiet_zone': 15.0, 'dpi': 300}\n"
    "    fn = barcode.get('ean13', '590123412345',\n"
    "                     writer=ImageWriter()).save(P('barcode_raw'), options=bopts)\n"
    "    b = cv2.imread(fn)\n"
    "    b = cv2.copyMakeBorder(b, 20, 20, 20, 20, cv2.BORDER_CONSTANT,\n"
    "                           value=(255, 255, 255))\n"
    "    cv2.imwrite(P('barcode.png'), b)\n"
    "except Exception as e:\n"
    "    sys.stderr.write('no barcode: %s\\n' % e)\n"
    "try:\n"
    "    import shutil\n"
    "    for rn in ('bottle_1.jpg', 'bottle_2.jpg', 'book.jpg'):\n"
    "        shutil.copy('/opt/cv-barcode-ref/' + rn, P(rn))\n"
    "except Exception as e:\n"
    "    sys.stderr.write('no barcode refs: %s\\n' % e)\n"
    "U = np.full((480, 640, 3), 240, np.uint8)\n"
    "cv2.rectangle(U, (0, 0), (640, 48), (120, 60, 30), -1)\n"
    "cv2.putText(U, 'Settings', (16, 34),\n"
    "            cv2.FONT_HERSHEY_SIMPLEX, 1.0, (255, 255, 255), 2, cv2.LINE_AA)\n"
    "cv2.rectangle(U, (500, 64), (600, 104), (0, 200, 0), -1)\n"
    "cv2.rectangle(U, (40, 120), (200, 180), (180, 110, 40), -1)\n"
    "cv2.putText(U, 'OK', (95, 162),\n"
    "            cv2.FONT_HERSHEY_SIMPLEX, 1.2, (255, 255, 255), 3, cv2.LINE_AA)\n"
    "cv2.putText(U, 'SYSTEM ONLINE', (40, 300),\n"
    "            cv2.FONT_HERSHEY_SIMPLEX, 1.4, (0, 0, 0), 3, cv2.LINE_AA)\n"
    "try:\n"
    "    q = cv2.resize(mk_qr('ui:ready', 4), (140, 140),\n"
    "                   interpolation=cv2.INTER_NEAREST)\n"
    "    U[320:460, 470:610] = q\n"
    "except Exception as e:\n"
    "    sys.stderr.write('no UI QR: %s\\n' % e)\n"
    "cv2.imwrite(P('ui.png'), U)\n"
    "cv2.imwrite(P('ui_ok_tpl.png'), U[120:180, 40:200].copy())\n"
    "open(P('bad_empty.png'), 'wb').close()\n"
    "with open(P('bad_text.png'), 'w') as f: f.write('this is not an image\\n')\n"
    "np.random.RandomState(7).randint(0, 256, 4096).astype(np.uint8)"
    ".tofile(P('bad_random.png'))\n"
    "with open(P('scene.png'), 'rb') as f: raw = f.read()\n"
    "with open(P('bad_truncated.png'), 'wb') as f: f.write(raw[:max(16, len(raw)//3)])\n"
    "cv2.imwrite(P('tiny.png'), np.full((1, 1, 3), 128, np.uint8))\n"
    "cv2.imwrite(P('wide.png'), np.full((1, 2000, 3), 200, np.uint8))\n"
    "rtp = ['RT-0000', 'hello world', 'https://x.example/p?q=1',\n"
    "       '0123456789', 'mix_Aa-9']\n"
    "for i, s in enumerate(rtp):\n"
    "    try:\n"
    "        cv2.imwrite(P('qr_rt_%d.png' % i), mk_qr(s, 10))\n"
    "    except Exception as e:\n"
    "        sys.stderr.write('no qr_rt_%d: %s\\n' % (i, e))\n"
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
