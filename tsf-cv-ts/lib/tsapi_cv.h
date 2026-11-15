/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Suite helpers
 *
 * Every test here needs the same two things: an agent with an RPC
 * server on it (tapi_cv drives OpenCV over that server), and a set of
 * deterministic images to look at. This makes the session and
 * generates the images on the agent, where tapi_cv then reads them.
 *
 * The fixtures are drawn by a small cv2/numpy script written onto the
 * agent and run there (the same idea as tsf-ai-ts's mock provider), so
 * no image is committed to the repository and the scene is known
 * exactly - its geometry and contents are the macros below, which the
 * tests assert against.
 *
 * @author Maxim Menshikov <maxim.menshikov@interpretica.io>
 */

#ifndef __TSAPI_CV_H__
#define __TSAPI_CV_H__

#include "rcf_rpc.h"
#include "te_errno.h"
#include "te_string.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The agent this suite works on. */
#define TSAPI_CV_TA   "Agt_A"

/* ---- the synthetic scene (scene.png), 320x240, white background ---- */

#define TSAPI_CV_SCENE_W    320  /**< Scene width. */
#define TSAPI_CV_SCENE_H    240  /**< Scene height. */

/** Filled red rectangle - also cropped out as the match template. */
#define TSAPI_CV_RED_X      40
#define TSAPI_CV_RED_Y      40
#define TSAPI_CV_RED_W      80
#define TSAPI_CV_RED_H      60

/** Filled green rectangle - the "indicator" the colour test measures. */
#define TSAPI_CV_GREEN_X    200
#define TSAPI_CV_GREEN_Y    60
#define TSAPI_CV_GREEN_W    60
#define TSAPI_CV_GREEN_H    60

/** The extra black rectangle scene_diff.png adds over scene.png. */
#define TSAPI_CV_DIFF_X     150
#define TSAPI_CV_DIFF_Y     150
#define TSAPI_CV_DIFF_W     30
#define TSAPI_CV_DIFF_H     20

/** The string rendered in text.png (and along the bottom of the scene). */
#define TSAPI_CV_OCR_TEXT   "HELLO CV 2026"

/** The payload encoded in qr.png (absent if the agent cannot encode QR). */
#define TSAPI_CV_QR_TEXT    "tsf-cv:ok"

/** What a test needs: an agent, an RPC server, and the fixture dir. */
typedef struct tsapi_cv_session {
    /** Agent name. */
    const char *ta;
    /** RPC server on it - what tapi_cv takes. */
    rcf_rpc_server *pco;
    /** Directory on the agent that holds the generated images. */
    char *dir;
} tsapi_cv_session;

/**
 * Open a session: an RPC server on the agent.
 *
 * @param[out] session  Session.
 * @param[in]  name     Name for the RPC server, unique within the test.
 *
 * @return Status code.
 */
extern te_errno tsapi_cv_session_init(tsapi_cv_session *session,
                                      const char *name);

/**
 * Generate the fixture images on the agent.
 *
 * Writes the generator onto the agent and runs it; on success
 * @c session->dir names the directory of images, and tsapi_cv_fixture()
 * builds a path to one. Returns @c TE_EOPNOTSUPP when the agent has no
 * usable python3+cv2 to draw them, so a test can skip.
 *
 * @param session   Session.
 *
 * @return Status code.
 */
extern te_errno tsapi_cv_fixtures(tsapi_cv_session *session);

/**
 * Build the path to a fixture image on the agent.
 *
 * @param[in]  session  Session whose fixtures were generated.
 * @param[in]  name     Basename, e.g. @c "scene.png".
 * @param[out] path     Appended with @c "<dir>/<name>".
 *
 * @return @c path->ptr.
 */
extern const char *tsapi_cv_fixture(const tsapi_cv_session *session,
                                    const char *name, te_string *path);

/**
 * Close a session.
 *
 * @param session       Session.
 */
extern void tsapi_cv_session_fini(tsapi_cv_session *session);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TSAPI_CV_H__ */
