import { memo, useState, useEffect, useRef } from 'react';
import { SKSE_API, log } from '../lib/skse-api';

interface ManualEntryModalProps {
  isOpen: boolean;
  onClose: () => void;
  isWhitelist?: boolean;
}

interface DetectionResult {
  type: 'topic' | 'scene' | 'plugin';
  categories: string[];
}

export const ManualEntryModal = memo(({ isOpen, onClose, isWhitelist = false }: ManualEntryModalProps) => {
  const [identifier, setIdentifier] = useState('');
  const [blockType, setBlockType] = useState<'Soft' | 'Hard'>('Soft');
  const [notes, setNotes] = useState('');
  const [detectedType, setDetectedType] = useState<'topic' | 'scene' | 'plugin'>('topic');
  const [categories, setCategories] = useState<string[]>(['Blacklist']);
  const [selectedCategory, setSelectedCategory] = useState('Blacklist');
  const detectionTimeoutRef = useRef<number | null>(null);

  // Set up global handler for detection results
  useEffect(() => {
    (window as any).handleIdentifierDetection = (result: DetectionResult) => {
      log(`[ManualEntry] Detection result: type=${result.type}, categories=${result.categories.length}`);
      setDetectedType(result.type);
      setCategories(result.categories);
      
      // Reset to "Blacklist" if current selection is not in new categories
      if (!result.categories.includes(selectedCategory)) {
        setSelectedCategory('Blacklist');
      }
    };

    return () => {
      delete (window as any).handleIdentifierDetection;
    };
  }, [selectedCategory]);

  // Detect identifier type when it changes (debounced)
  useEffect(() => {
    if (detectionTimeoutRef.current) {
      clearTimeout(detectionTimeoutRef.current);
    }

    if (identifier.trim()) {
      detectionTimeoutRef.current = window.setTimeout(() => {
        log(`[ManualEntry] Detecting type for: ${identifier}`);
        SKSE_API.sendToSKSE('detectIdentifierType', JSON.stringify({ identifier }));
      }, 300); // 300ms debounce
    } else {
      // Reset to default when empty
      setDetectedType('topic');
      setCategories(['Blacklist']);
      setSelectedCategory('Blacklist');
    }

    return () => {
      if (detectionTimeoutRef.current) {
        clearTimeout(detectionTimeoutRef.current);
      }
    };
  }, [identifier]);

  // Reset form when modal opens/closes
  useEffect(() => {
    if (!isOpen) {
      setIdentifier('');
      setNotes('');
      setBlockType('Soft');
      setDetectedType('topic');
      setCategories(['Blacklist']);
      setSelectedCategory('Blacklist');
    }
  }, [isOpen]);

  // Handle ESC key to close modal
  useEffect(() => {
    if (!isOpen) return;

    const handleEsc = (e: KeyboardEvent) => {
      if (e.keyCode === 27 || e.key === 'Escape') {
        e.stopImmediatePropagation();
        e.preventDefault();
        onClose();
      }
    };

    window.addEventListener('keydown', handleEsc, { capture: true });
    return () => window.removeEventListener('keydown', handleEsc, { capture: true });
  }, [isOpen, onClose]);

  if (!isOpen) return null;

  const handleBackdropClick = (e: React.MouseEvent) => {
    if (e.target === e.currentTarget) {
      onClose();
    }
  };

  const handleCreate = () => {
    if (!identifier.trim()) {
      log('[ManualEntry] Empty identifier, aborting');
      return;
    }

    log(`[ManualEntry] Creating entry: identifier=${identifier}, blockType=${blockType}, category=${isWhitelist ? 'Whitelist' : selectedCategory}, isWhitelist=${isWhitelist}`);

    // Call new createManualEntry handler
    SKSE_API.sendToSKSE('createManualEntry', JSON.stringify({
      identifier,
      blockType,
      category: isWhitelist ? 'Whitelist' : selectedCategory,
      notes,
      isWhitelist
    }));

    // Close modal
    onClose();
  };

  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center bg-black bg-opacity-75"
      onClick={handleBackdropClick}
      role="dialog"
      aria-modal="true"
      aria-labelledby="modal-title"
    >
      <div className="bg-gray-800 border border-gray-700 rounded-lg shadow-2xl w-full max-w-2xl flex flex-col">
        {/* Header */}
        <div className="flex items-center justify-between p-4 border-b border-gray-700">
          <h2 id="modal-title" className="text-xl font-bold text-white">
            {isWhitelist ? 'Create Whitelist Entry' : 'Create Blacklist Entry'}
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
        <div className="p-6 space-y-4">
          {/* Identifier Input */}
          <div>
            <label className="block text-base font-medium text-gray-300 mb-2">
              Identifier
            </label>
            <input
              type="text"
              value={identifier}
              onChange={(e) => setIdentifier(e.target.value)}
              placeholder={isWhitelist ? "EditorID, FormID, or Plugin.esp" : "EditorID or FormID (e.g., DragonBridgeFarmScene02 or 02707A)"}
              className="w-full px-4 py-2.5 text-base bg-gray-700 text-white rounded-lg border border-gray-600 focus:outline-none focus:border-blue-500"
              autoFocus
            />
            <div className="text-sm text-gray-400 mt-1">
              {detectedType === 'scene' && '🎬 Detected as Scene'}
              {detectedType === 'topic' && '💬 Detected as Topic'}
              {detectedType === 'plugin' && '📦 Detected as Plugin'}
            </div>
          </div>

          {/* Block Type - only for blacklist */}
          {!isWhitelist && (
          <div>
            <label className="block text-base font-medium text-gray-300 mb-2">
              Block Type
            </label>
            <div className="flex gap-4">
              <label className="flex items-center gap-2 cursor-pointer">
                <input
                  type="radio"
                  name="blockType"
                  checked={blockType === 'Soft'}
                  onChange={() => setBlockType('Soft')}
                  className="w-5 h-5 text-blue-600 focus:ring-blue-500 cursor-pointer"
                />
                <span className="text-base text-white">Soft Block</span>
              </label>
              <label className="flex items-center gap-2 cursor-pointer">
                <input
                  type="radio"
                  name="blockType"
                  checked={blockType === 'Hard'}
                  onChange={() => setBlockType('Hard')}
                  className="w-5 h-5 text-blue-600 focus:ring-blue-500 cursor-pointer"
                />
                <span className="text-base text-white">Hard Block</span>
              </label>
            </div>
            <div className="text-sm text-gray-400 mt-1">
              Soft blocks mute audio and hide subtitles. Hard blocks prevent dialogue before it plays.
            </div>
          </div>
          )}

          {/* Filter Category - only for blacklist */}
          {!isWhitelist && (
          <div>
            <label className="block text-base font-medium text-gray-300 mb-2">
              Filter Category
            </label>
            <select
              value={selectedCategory}
              onChange={(e) => setSelectedCategory(e.target.value)}
              className="w-full px-4 py-2.5 text-base bg-gray-700 text-white rounded-lg border border-gray-600 focus:outline-none focus:border-blue-500"
            >
              {categories.map((category) => (
                <option key={category} value={category}>
                  {category}
                </option>
              ))}
            </select>
            <div className="text-sm text-gray-400 mt-1">
              Categories change based on detected type
            </div>
          </div>
          )}

          {/* Notes */}
          <div>
            <label className="block text-base font-medium text-gray-300 mb-2">
              Notes (Optional)
            </label>
            <textarea
              value={notes}
              onChange={(e) => setNotes(e.target.value)}
              placeholder="Add notes about why this entry was blocked..."
              rows={3}
              className="w-full px-4 py-2.5 text-base bg-gray-700 text-white rounded-lg border border-gray-600 focus:outline-none focus:border-blue-500 resize-none"
            />
          </div>
        </div>

        {/* Footer */}
        <div className="p-4 border-t border-gray-700 flex justify-end gap-3">
          <button
            onClick={onClose}
            className="px-6 py-2.5 text-base bg-gray-700 hover:bg-gray-600 text-white rounded-lg transition-colors"
          >
            Cancel
          </button>
          <button
            onClick={handleCreate}
            disabled={!identifier.trim()}
            className="px-6 py-2.5 text-base bg-green-600 hover:bg-green-700 disabled:bg-gray-600 disabled:cursor-not-allowed text-white rounded-lg transition-colors"
          >
            Create Entry
          </button>
        </div>
      </div>
    </div>
  );
});

ManualEntryModal.displayName = 'ManualEntryModal';

