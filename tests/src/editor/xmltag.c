/*
   src/editor - xml tag highlighting tests.

   Copyright (C) 2012
   The Free Software Foundation, Inc.

   Written by:
   Slava Zanko  <slavazanko@gmail.com>, 2012

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define TEST_SUITE_NAME "/src/editor"

#include <config.h>

#include <check.h>

#include <stdio.h>

#include "lib/global.h"

#include "src/vfs/local/local.c"
#include "src/editor/xml-tag.c"
#include "src/editor/edit.c"

/* --------------------------------------------------------------------------------------------- */

static WEdit *edit;
/* --------------------------------------------------------------------------------------------- */

static void
setup_vfs(void)
{
    str_init_strings (NULL);

    vfs_init ();
    init_localfs ();
    vfs_setup_work_dir ();
}

static void
setup (void)
{
    setup_vfs ();

    visualize_tags = TRUE;
    option_save_position = FALSE;

    edit = edit_init (NULL, 0, 0, 24, 80, vfs_path_from_str ("test-data.xml"), 1);
}

static void
teardown_vfs (void)
{
    vfs_shut ();
    str_uninit_strings ();
}

static void
teardown (void)
{
    edit_clean (edit);
    g_free (edit);

    teardown_vfs ();
}

/* --------------------------------------------------------------------------------------------- */

void
edit_load_syntax (WEdit * _edit, char ***_pnames, const char *_type)
{
    (void) _edit;
    (void) _pnames;
    (void) _type;

}

int
edit_get_syntax_color (WEdit * _edit, off_t _byte_index)
{
    (void) _edit;
    (void) _byte_index;

    return 0;
}

gboolean
edit_load_macro_cmd (WEdit * _edit) {
    (void) _edit;

    return FALSE;
}
/* --------------------------------------------------------------------------------------------- */

START_TEST (test_if_visualize_tags_is_respected)
{
    // given
    visualize_tags = FALSE;
    edit->force = 0;

    // when
    edit_find_xmlpair (edit);

    // then
    fail_unless ((edit->force & REDRAW_PAGE) == 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

static struct data_source_1 {
    off_t input_position;

    off_t expected_open_pos;
    size_t expected_open_len;
    off_t expected_close_pos;
    size_t expected_close_len;
} ds_1[] = {
    {1, 0, 6, 915, 7},          /*0*/  /*  1: <test> */
    {14, 12, 9, 275, 10},       /*1*/  /*  3: <modules> */
    {33, 30, 8, 42, 9},         /*2*/  /*  4: <module>mod1</module> */
    {62, 61, 8, 261, 9},        /*3*/  /*  6: <module> */
    {61, 0, 0, 0, 0},           /*4*/  /*  6: <module> */
    {183, 82, 8, 179, 9},       /*5*/  /* 11: </module> */
    {342, 0, 0, 0, 0},          /*6*/  /* 20: <dependency /> */
    {404, 0, 0, 0, 0},          /*7*/  /* 24: <invalid1> */
    {630, 0, 0, 0, 0},          /*8*/  /* 41: </invalid2> */
    {858, 854, 10, 886, 11},    /*9*/  /* 57: <invalid3> */
    {905, 870, 10, 903, 11},    /*10*/ /* 63: </invalid4> */
    {925, 924, 10, 938, 11},    /*11*/ /* 66: <тест>test</тест> */
    {951, 950, 11, 965, 12},    /*12*/ /* 67: <テスト>test</テスト> */
    {978, 979, 10, 992, 11},    /*13*/ /* 68: <testTest>test</testTest> */
    {1005, 1004, 11, 1019, 12}, /*14*/ /* 69: <test-test>test</test-test> */
    {1033, 1032, 11, 1047, 12}, /*15*/ /* 70: <test:test>test</test:test> */
    {1061, 1060, 11, 1075, 12}, /*16*/ /* 71: <test.test>test</test.test> */
    {1089, 1088, 11, 1103, 12}, /*17*/ /* 72: <test_test>test</test-test> */
    {1117, 1116, 13, 1133, 14}, /*18*/ /* 73: <01234567890>test</01234567890> */
    {1151, 0, 0, 0, 0},         /*19*/ /* 76: <invalid.*>test</invalid.*> */
    {1179, 0, 0, 0, 0},         /*20*/ /* 77: <invalid+test>test</invalid+test> */
    {1213, 0, 0, 0, 0},         /*21*/ /* 78: <invalid=test>test</invalid=test> */
    {1247, 0, 0, 0, 0},         /*22*/ /* 79: <invalid&test>test</invalid&test> */
    {1281, 0, 0, 0, 0},         /*23*/ /* 80: <Testtest>test</testTest> */
};
START_TEST (test_if_pairs_found)
{
    // given
    struct data_source_1 *ds = &ds_1[_i];

    edit_load_file (edit);

    // when
    edit_cursor_move (edit, ds->input_position);
    edit_find_xmlpair (edit);

    // then
    fail_if ((edit->force & REDRAW_PAGE) == 0);
    ck_assert_int_eq (edit->xmltag.open.pos, ds->expected_open_pos);
    ck_assert_int_eq (edit->xmltag.open.len, ds->expected_open_len);
    ck_assert_int_eq (edit->xmltag.close.pos, ds->expected_close_pos);
    ck_assert_int_eq (edit->xmltag.close.len, ds->expected_close_len);
}
END_TEST


/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    int number_failed;

    Suite *s = suite_create (TEST_SUITE_NAME);
    TCase *tc_core = tcase_create ("Core");
    SRunner *sr;

    tcase_add_checked_fixture (tc_core, setup, teardown);

    /* Add new tests here: *************** */
    tcase_add_test (tc_core, test_if_visualize_tags_is_respected);
    tcase_add_loop_test(tc_core, test_if_pairs_found, 0, sizeof(ds_1)/sizeof(ds_1[0]));
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "xmltag.log");
    srunner_run_all (sr, CK_NOFORK);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);

    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
