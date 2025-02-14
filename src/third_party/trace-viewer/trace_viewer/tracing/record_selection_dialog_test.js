// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

tvcm.require('tvcm.settings');
tvcm.require('tracing.test_utils');
tvcm.require('tracing.record_selection_dialog');

tvcm.unittest.testSuite('tracing.record_selection_dialog_test', function() {
  test('instantitate', function() {
    var categories = [];
    for (var i = 0; i < 30; i++)
      categories.push('cat-' + i);
    for (var i = 0; i < 20; i++)
      categories.push('disabled-by-default-cat-' + i);
    categories.push('really-really-really-really-really-really-very-loong-cat');
    categories.push('first,second,third');
    categories.push('cc,disabled-by-default-cc.debug');

    var dlg = new tracing.RecordSelectionDialog();
    dlg.categories = categories;
    dlg.settings_key = 'key';

    var showButton = document.createElement('button');
    showButton.textContent = 'Show record selection dialog';
    showButton.addEventListener('click', function(e) {
      dlg.visible = true;
      e.stopPropagation();
    });
    this.addHTMLOutput(showButton);
  });

  test('recordSelectionDialog_splitCategories', function() {
    var dlg = new tracing.RecordSelectionDialog();
    dlg.categories =
        ['cc,disabled-by-default-one,cc.debug', 'two,three', 'three'];
    dlg.updateForm_();

    var expected =
        ['"cc"', '"cc.debug"', '"disabled-by-default-one"', '"three"', '"two"'];

    var labels = dlg.getElementsByTagName('input');
    var results = [];
    for (var i = 0; i < labels.length; i++) {
      results.push('"' + labels[i].value + '"');
    }
    results = results.sort();

    assertArrayEquals(expected, results);
  });

  test('recordSelectionDialog_UpdateForm_NoSettings', function() {
    var dlg = new tracing.RecordSelectionDialog();
    dlg.categories = ['disabled-by-default-one', 'two', 'three'];
    dlg.settings_key = 'key';
    dlg.updateForm_();

    var checkboxes = dlg.getElementsByTagName('input');
    assertEquals(3, checkboxes.length);
    assertEquals('three', checkboxes[0].id);
    assertEquals('three', checkboxes[0].value);
    assertEquals(true, checkboxes[0].checked);
    assertEquals('two', checkboxes[1].id);
    assertEquals('two', checkboxes[1].value);
    assertEquals(true, checkboxes[1].checked);
    assertEquals('disabled-by-default-one', checkboxes[2].id);
    assertEquals('disabled-by-default-one', checkboxes[2].value);
    assertEquals(false, checkboxes[2].checked);

    assertEquals('', dlg.categoryFilter());

    var labels = dlg.getElementsByTagName('label');
    assertEquals(3, labels.length);
    assertEquals('three', labels[0].textContent);
    assertEquals('two', labels[1].textContent);
    assertEquals('one', labels[2].textContent);
  });

  test('recordSelectionDialog_UpdateForm_Settings', function() {
    tvcm.Settings.set('two', true, 'categories');
    tvcm.Settings.set('three', false, 'categories');

    var dlg = new tracing.RecordSelectionDialog();
    dlg.categories = ['disabled-by-default-one'];
    dlg.settings_key = 'categories';
    dlg.updateForm_();

    var checkboxes = dlg.getElementsByTagName('input');
    assertEquals(3, checkboxes.length);
    assertEquals('three', checkboxes[0].id);
    assertEquals('three', checkboxes[0].value);
    assertEquals(false, checkboxes[0].checked);
    assertEquals('two', checkboxes[1].id);
    assertEquals('two', checkboxes[1].value);
    assertEquals(true, checkboxes[1].checked);
    assertEquals('disabled-by-default-one', checkboxes[2].id);
    assertEquals('disabled-by-default-one', checkboxes[2].value);
    assertEquals(false, checkboxes[2].checked);

    assertEquals('-three', dlg.categoryFilter());

    var labels = dlg.getElementsByTagName('label');
    assertEquals(3, labels.length);
    assertEquals('three', labels[0].textContent);
    assertEquals('two', labels[1].textContent);
    assertEquals('one', labels[2].textContent);
  });

  test('recordSelectionDialog_UpdateForm_DisabledByDefault', function() {
    var dlg = new tracing.RecordSelectionDialog();
    dlg.categories = ['disabled-by-default-bar', 'baz'];
    dlg.settings_key = 'categories';
    dlg.updateForm_();

    assertEquals('', dlg.categoryFilter());

    var inputs =
        dlg.querySelector('input#disabled-by-default-bar').click();

    assertEquals('disabled-by-default-bar', dlg.categoryFilter());

    assertEquals(false,
        tvcm.Settings.get('disabled-by-default-foo', false, 'categories'));
  });

  test('selectAll', function() {
    tvcm.Settings.set('two', true, 'categories');
    tvcm.Settings.set('three', false, 'categories');

    var dlg = new tracing.RecordSelectionDialog();
    dlg.categories = ['disabled-by-default-one'];
    dlg.settings_key = 'categories';
    dlg.updateForm_();
  });

  test('selectNone', function() {
    tvcm.Settings.set('two', true, 'categories');
    tvcm.Settings.set('three', false, 'categories');

    var dlg = new tracing.RecordSelectionDialog();
    dlg.categories = ['disabled-by-default-one'];
    dlg.settings_key = 'categories';
    dlg.updateForm_();

    // Enables the three option, two already enabled.
    dlg.querySelector('.default-enabled-categories .all-btn').click();
    assertEquals('', dlg.categoryFilter());
    assertEquals(true, tvcm.Settings.get('three', false, 'categories'));

    // Disables three and two.
    dlg.querySelector('.default-enabled-categories .none-btn').click();
    assertEquals('-three,-two', dlg.categoryFilter());
    assertEquals(false, tvcm.Settings.get('two', false, 'categories'));
    assertEquals(false, tvcm.Settings.get('three', false, 'categories'));

    // Turn categories back on so they can be ignored.
    dlg.querySelector('.default-enabled-categories .all-btn').click();

    // Enables disabled category.
    dlg.querySelector('.default-disabled-categories .all-btn').click();
    assertEquals('disabled-by-default-one', dlg.categoryFilter());
    assertEquals(true,
        tvcm.Settings.get('disabled-by-default-one', false, 'categories'));

    // Turn disabled by default back off.
    dlg.querySelector('.default-disabled-categories .none-btn').click();
    assertEquals('', dlg.categoryFilter());
    assertEquals(false,
        tvcm.Settings.get('disabled-by-default-one', false, 'categories'));
  });
});
