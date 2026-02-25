import { create } from 'zustand';

export type ToastType = 'success' | 'error' | 'info';

interface Toast {
  id: number;
  message: string;
  type: ToastType;
}

interface ToastStore {
  toasts: Toast[];
  addToast: (message: string, type: ToastType) => void;
  removeToast: (id: number) => void;
}

let nextId = 1;

export const useToastStore = create<ToastStore>((set) => ({
  toasts: [],
  addToast: (message, type) => {
    const id = nextId++;
    set(state => ({
      toasts: [...state.toasts, { id, message, type }]
    }));
  },
  removeToast: (id) => {
    set(state => ({
      toasts: state.toasts.filter(t => t.id !== id)
    }));
  }
}));

// Global function for C++ to call
if (typeof window !== 'undefined') {
  (window as any).showToast = (message: string, type: ToastType = 'info') => {
    useToastStore.getState().addToast(message, type);
  };
}
