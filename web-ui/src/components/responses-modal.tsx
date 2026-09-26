import { memo } from 'react';
import { Modal } from './modal';

interface ResponsesModalProps {
  isOpen: boolean;
  onClose: () => void;
  title: string;
  responses: string[];
}

export const ResponsesModal = memo(({ isOpen, onClose, title, responses }: ResponsesModalProps) => (
  <Modal isOpen={isOpen} onClose={onClose} title={title} sizeClassName="max-w-3xl max-h-[80vh]">
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
  </Modal>
));

ResponsesModal.displayName = 'ResponsesModal';
