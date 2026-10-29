import test from 'node:test';
import assert from 'node:assert/strict';
import {PRIMARY_PAGES, parentPage, primaryPage, adjacentPage, swipePage} from '../dist/navigation.js';

test('the display has four agreed primary pages', () => {
  assert.deepEqual(PRIMARY_PAGES.map(page => page.label), ['Главная', 'Управление', 'Настройки', 'Сервис']);
});
test('fuel, history, journal and diagnostics keep their parent hierarchy', () => {
  assert.equal(parentPage('fuel'), 'overview');
  assert.equal(parentPage('journal'), 'fuel');
  assert.equal(parentPage('diagnostics'), 'fuel');
  assert.equal(parentPage('history', 'fuel'), 'fuel');
  assert.equal(parentPage('history', 'room'), 'overview');
  assert.equal(primaryPage('journal'), 'overview');
  for (const route of ['wifi', 'bluetooth', 'datetime', 'display']) {
    assert.equal(parentPage(route), 'settings');
    assert.equal(primaryPage(route), 'settings');
  }
});
test('primary navigation wraps in both directions', () => {
  assert.equal(adjacentPage('overview', -1), 'service');
  assert.equal(adjacentPage('service', 1), 'overview');
  assert.equal(adjacentPage('overview', 1), 'controls');
  assert.equal(adjacentPage('settings', -1), 'controls');
});
test('only a deliberate horizontal gesture changes a primary page', () => {
  assert.equal(swipePage('overview', -140, 15, 300), 'controls');
  assert.equal(swipePage('controls', 140, -15, 300), 'overview');
  for (const [dx, dy, elapsed] of [[-30, 0, 100], [20, 160, 200], [80, 70, 200], [90, 0, 1500], [NaN, 0, 200]]) {
    assert.equal(swipePage('overview', dx, dy, elapsed), null);
  }
});
test('detail screens and invalid directions cannot become swipe commands', () => {
  for (const route of ['fuel', 'history', 'journal', 'diagnostics', 'wifi']) {
    assert.equal(swipePage(route, -150, 0, 200), null);
  }
  assert.equal(adjacentPage('overview', 0), null);
  assert.equal(adjacentPage('unknown', 1), null);
});
