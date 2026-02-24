import { memo } from 'react';

interface ResponsesModalProps {
  isOpen: boolean;
  onClose: () => void;
  title: string;
  responses: string[];
}

export const ResponsesModal = memo(({ isOpen, onClose, title, responses }: ResponsesModalProps) => {
  if (!isOpen) return null;

  const handleBackdropClick = (e: React.MouseEvent) => {
    if (e.target === e.currentTarget) {
      onClose();
    }
  };

  const handleEscKey = (e: React.KeyboardEvent) => {
    if (e.key === 'Escape') {
      onClose();
    }
  };

  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center bg-black bg-opacity-75"
      onClick={handleBackdropClick}
      onKeyDown={handleEscKey}
      role="dialog"
      aria-modal="true"
      aria-labelledby="modal-title"
    >
      <div className="bg-gray-800 border border-gray-700 rounded-lg shadow-2xl w-full max-w-3xl max-h-[80vh] flex flex-col">
        {/* Header */}
        <div className="flex items-center justify-between p-4 border-b border-gray-700">
          <h2 id="modal-title" className="text-xl font-bold text-white">
            {title}
          </h2>
          <button
            onClick={onClose}
            className="text-gray-400 hover:text-white transition-colors"
            aria-label="Close modal"
          >
            <svg className="w-6 h-6" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M6 18L18 6M6 6l12 12" />
            </svg>
          </button>
        </div>

        {/* Content */}
        <div className="flex-1 overflow-y-auto p-4">
          {responses.length === 0 ? (
            <div className="text-gray-500 text-center py-8">
              No responses have been captured yet.
              <div className="text-sm mt-2">
                Responses will be captured when this dialogue plays in-game.
              </div>
            </div>
          ) : (
            <div className="space-y-2">
              {responses.map((response, index) => (
                <div
                  key={index}
                  className={`border border-gray-700 rounded p-3 hover:border-gray-600 transition-colors ${
                    index % 2 === 0 ? 'bg-gray-850' : 'bg-gray-800'
                  }`}
                >
                  <div className="text-sm text-gray-400 mb-1">Response {index + 1}</div>
                  <div className="text-base text-white whitespace-pre-wrap break-words">
                    {response}
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>

        {/* Footer */}
        <div className="p-4 border-t border-gray-700 text-center">
          <div className="text-sm text-gray-400">
            {responses.length} {responses.length === 1 ? 'response' : 'responses'} captured
          </div>
          <div className="text-xs text-gray-500 mt-1">Press ESC or click outside to close</div>
        </div>
      </div>
    </div>
  );
});

ResponsesModal.displayName = 'ResponsesModal';
