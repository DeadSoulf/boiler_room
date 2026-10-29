/** Display navigation. Detail screens retain their parent; gestures never issue commands. */
export const PRIMARY_PAGES = Object.freeze([
  {id: 'overview', label: 'Главная', icon: 'grid'},
  {id: 'controls', label: 'Управление', icon: 'power'},
  {id: 'settings', label: 'Настройки', icon: 'sliders'},
  {id: 'service', label: 'Сервис', icon: 'service'},
]);

const PARENTS = Object.freeze({
  fuel: 'overview', history: 'overview', journal: 'fuel', diagnostics: 'fuel',
  wifi: 'settings', bluetooth: 'settings', datetime: 'settings', display: 'settings',
});

export function parentPage(route, historySensor = '') {
  return route === 'history' && historySensor === 'fuel' ? 'fuel' : PARENTS[route] || null;
}

export function primaryPage(route) {
  let current = route;
  while (PARENTS[current]) current = PARENTS[current];
  return PRIMARY_PAGES.some(page => page.id === current) ? current : 'overview';
}

export function adjacentPage(route, direction) {
  const index = PRIMARY_PAGES.findIndex(page => page.id === route);
  if (index < 0 || ![-1, 1].includes(direction)) return null;
  return PRIMARY_PAGES[(index + direction + PRIMARY_PAGES.length) % PRIMARY_PAGES.length].id;
}

export function swipePage(route, dx, dy, duration) {
  if (![dx, dy, duration].every(Number.isFinite) || duration < 0 || duration > 1000) return null;
  if (Math.abs(dx) < 60 || Math.abs(dx) < Math.abs(dy) * 1.5) return null;
  return adjacentPage(route, dx < 0 ? 1 : -1);
}
