import { useState, useRef, useEffect, useMemo, useCallback } from 'react';

const PAGE_SIZE = 100;

// Renders a long list in pages of PAGE_SIZE, loading the next page when the
// sentinel element scrolls into view. Returns to the first page whenever
// resetKey changes (pass something derived from the active filters).
export function useLazyList<T>(items: T[], resetKey: unknown) {
  const [displayCount, setDisplayCount] = useState(PAGE_SIZE);
  const sentinelRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const sentinel = sentinelRef.current;
    if (!sentinel) return;

    const observer = new IntersectionObserver(
      (observed) => {
        if (observed[0].isIntersecting && displayCount < items.length) {
          setDisplayCount(prev => Math.min(prev + PAGE_SIZE, items.length));
        }
      },
      { threshold: 0.5, rootMargin: '200px' } // Load earlier with margin
    );

    observer.observe(sentinel);
    return () => observer.disconnect();
  }, [displayCount, items.length]);

  useEffect(() => {
    setDisplayCount(PAGE_SIZE);
  }, [resetKey]);

  const displayed = useMemo(() => items.slice(0, displayCount), [items, displayCount]);

  return { displayed, displayCount, sentinelRef };
}

// Click handling for multi-select lists: a plain click selects one item,
// Ctrl/Cmd-click toggles an item, Shift-click selects the range from the last
// clicked item. `index` is the item's position in `items`.
export function useMultiSelect<T extends { id: number }>(
  items: T[],
  selected: T[],
  setSelected: (entries: T[]) => void
) {
  const [lastClickedIndex, setLastClickedIndex] = useState(-1);

  const handleItemClick = useCallback((entry: T, index: number, event?: React.MouseEvent) => {
    const isCtrlClick = event?.ctrlKey || event?.metaKey;
    const isShiftClick = event?.shiftKey;

    if (isShiftClick && lastClickedIndex >= 0 && items.length > 0) {
      const start = Math.min(lastClickedIndex, index);
      const end = Math.max(lastClickedIndex, index);
      setSelected(items.slice(start, end + 1));
    } else if (isCtrlClick) {
      const isSelected = selected.some(e => e.id === entry.id);
      setSelected(isSelected ? selected.filter(e => e.id !== entry.id) : [...selected, entry]);
      setLastClickedIndex(index);
    } else {
      setSelected([entry]);
      setLastClickedIndex(index);
    }
  }, [lastClickedIndex, items, selected, setSelected]);

  const clearAnchor = useCallback(() => setLastClickedIndex(-1), []);

  return { handleItemClick, clearAnchor };
}
