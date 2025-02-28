/*
 * Copyright (C) 2016 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WI.CollectionContentView = class CollectionContentView extends WI.ContentView
{
    constructor(collection, contentViewConstructor, contentPlaceholder)
    {
        console.assert(collection instanceof WI.Collection);
        console.assert(contentViewConstructor instanceof WI.ContentView);

        super(collection);

        this.element.classList.add("collection");

        this._contentPlaceholder = contentPlaceholder || collection.displayName;
        this._contentPlaceholderElement = null;
        this._contentViewConstructor = contentViewConstructor;
        this._contentViewMap = new Map;
        this._handleClickMap = new WeakMap;
        this._selectedItem = null;
        this._selectionEnabled = false;
    }

    // Public

    get supplementalRepresentedObjects()
    {
        if (this._selectedItem)
            return [this._selectedItem];
        return [];
    }

    shown()
    {
        super.shown();

        for (let contentView of this._contentViewMap.values())
            contentView.shown();
    }

    hidden()
    {
        for (let contentView of this._contentViewMap.values())
            contentView.hidden();

        super.hidden();
    }

    get selectionEnabled()
    {
        return this._selectionEnabled;
    }

    set selectionEnabled(value)
    {
        if (this._selectionEnabled === value)
            return;

        this._selectionEnabled = value;
        if (!this._selectionEnabled)
            this._selectItem(null);
    }

    setSelectedItem(item)
    {
        console.assert(this._selectionEnabled, "Attempted to set selected item when selection is disabled.");
        if (!this._selectionEnabled)
            return;

        let contentView = this._contentViewMap.get(item);
        console.assert(contentView, "Missing contet view for item.", item);
        if (!contentView)
            return;

        this._selectItem(item);
        contentView.element.scrollIntoViewIfNeeded();
    }

    // Protected

    get contentViewConstructorOptions()
    {
        // Implemented by subclasses.
        return {};
    }

    addContentViewForItem(item)
    {
        if (!this._contentViewConstructor)
            return;

        if (this._contentViewMap.has(item)) {
            console.assert(false, "Already added ContentView for item.", item);
            return;
        }

        this.hideContentPlaceholder();

        let contentView = new this._contentViewConstructor(item, this.contentViewConstructorOptions);
        console.assert(contentView instanceof WI.ContentView);

        let handleClick = (event) => {
            if (event.button !== 0 || event.ctrlKey)
                return;

            if (this._selectionEnabled)
                this._selectItem(item);
            else
                WI.showRepresentedObject(item);
        };

        this._contentViewMap.set(item, contentView);
        this._handleClickMap.set(item, handleClick);
        contentView.element.addEventListener("click", handleClick);

        this.addSubview(contentView);
        this.contentViewAdded(contentView);

        if (!this.visible)
            return;

        contentView.visible = true;
        contentView.shown();
    }

    removeContentViewForItem(item)
    {
        if (!this._contentViewConstructor)
            return;

        let contentView = this._contentViewMap.get(item);
        console.assert(contentView);
        if (!contentView)
            return;

        if (this._selectedItem === item)
            this._selectItem(null);

        this.removeSubview(contentView);
        this._contentViewMap.delete(item);
        this.contentViewRemoved(contentView);

        if (this.visible) {
            contentView.visible = false;
            contentView.hidden();
        }

        contentView.removeEventListener(null, null, this);

        let handleClick = this._handleClickMap.get(item);
        console.assert(handleClick);

        if (handleClick) {
            contentView.element.removeEventListener("click", handleClick);
            this._handleClickMap.delete(item);
        }

        if (!this.subviews.length)
            this.showContentPlaceholder();
    }

    contentViewAdded(contentView)
    {
        // Implemented by subclasses.
    }

    contentViewRemoved(contentView)
    {
        // Implemented by subclasses.
    }

    showContentPlaceholder()
    {
        if (!this._contentPlaceholderElement) {
            if (typeof this._contentPlaceholder === "string")
                this._contentPlaceholderElement = WI.createMessageTextView(this._contentPlaceholder);
            else if (this._contentPlaceholder instanceof HTMLElement)
                this._contentPlaceholderElement = this._contentPlaceholder;
        }

        if (!this._contentPlaceholderElement.parentNode)
            this.element.appendChild(this._contentPlaceholderElement);
    }

    hideContentPlaceholder()
    {
        if (this._contentPlaceholderElement)
            this._contentPlaceholderElement.remove();
    }

    initialLayout()
    {
        if (!this.representedObject.size || !this._contentViewConstructor) {
            this.showContentPlaceholder();
            return;
        }
    }

    attached()
    {
        super.attached();

        this.representedObject.addEventListener(WI.Collection.Event.ItemAdded, this._handleItemAdded, this);
        this.representedObject.addEventListener(WI.Collection.Event.ItemRemoved, this._handleItemRemoved, this);

        for (let item of this._contentViewMap.keys()) {
            if (this.representedObject.has(item))
                continue;

            this.removeContentViewForItem(item);
            if (this._selectedItem === item)
                this._selectItem(null);
        }

        for (let item of this.representedObject) {
            if (!this._contentViewMap.has(item))
                this.addContentViewForItem(item);
        }
    }

    detached()
    {
        this.representedObject.removeEventListener(null, null, this);

        super.detached();
    }

     // Private

    _handleItemAdded(event)
    {
        let item = event.data.item;
        if (!item)
            return;

        this.addContentViewForItem(item);
    }

    _handleItemRemoved(event)
    {
        let item = event.data.item;
        if (!item)
            return;

        this.removeContentViewForItem(item);
    }

    _handleContentError(event)
    {
        if (event && event.target)
            this._removeContentViewForItem(event.target.representedObject);
    }

    _selectItem(item)
    {
        if (this._selectedItem === item)
            return;

        if (this._selectedItem) {
            let contentView = this._contentViewMap.get(this._selectedItem);
            console.assert(contentView, "Missing ContentView for deselected item.", this._selectedItem);
            contentView.element.classList.remove("selected");
        }

        this._selectedItem = item;

        if (this._selectedItem) {
            let selectedContentView = this._contentViewMap.get(this._selectedItem);
            console.assert(selectedContentView, "Missing ContentView for selected item.", this._selectedItem);
            selectedContentView.element.classList.add("selected");
        }

        this.dispatchEventToListeners(WI.ContentView.Event.SupplementalRepresentedObjectsDidChange);
    }
};
