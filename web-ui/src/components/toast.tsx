import { useEffect } from 'react';
import { useToastStore, type ToastType } from '@/stores/toast';
import { CheckCircle, AlertCircle, Info, X } from 'lucide-react';

export const Toast = () => {
  const { toasts, removeToast } = useToastStore();

  useEffect(() => {
    // Auto-dismiss toasts after 4 seconds
    const timers = toasts.map(toast => 
      setTimeout(() => removeToast(toast.id), 4000)
    );

    return () => timers.forEach(clearTimeout);
  }, [toasts, removeToast]);

  const getIcon = (type: ToastType) => {
    switch (type) {
      case 'success':
        return <CheckCircle className="w-5 h-5 text-green-400" />;
      case 'error':
        return <AlertCircle className="w-5 h-5 text-red-400" />;
      case 'info':
        return <Info className="w-5 h-5 text-blue-400" />;
    }
  };

  const getBorderColor = (type: ToastType) => {
    switch (type) {
      case 'success':
        return 'border-green-500';
      case 'error':
        return 'border-red-500';
      case 'info':
        return 'border-blue-500';
    }
  };

  if (toasts.length === 0) return null;

  return (
    <div className="fixed bottom-6 right-6 z-50 flex flex-col gap-3 pointer-events-none">
      {toasts.map(toast => (
        <div
          key={toast.id}
          className={`
            flex items-center gap-3 px-4 py-3 bg-gray-800 border-l-4 
            ${getBorderColor(toast.type)} rounded-lg shadow-lg 
            animate-in slide-in-from-right duration-300 pointer-events-auto
            min-w-[300px] max-w-[500px]
          `}
        >
          {getIcon(toast.type)}
          <span className="text-white flex-1">{toast.message}</span>
          <button
            onClick={() => removeToast(toast.id)}
            className="text-gray-400 hover:text-white transition-colors"
          >
            <X className="w-4 h-4" />
          </button>
        </div>
      ))}
    </div>
  );
};
