#pragma once

#include <gst/gstbin.h>
#include <gst/gstbus.h>
#include <gst/gstelement.h>
#include <gst/gstmessage.h>
#include <gst/gstpipeline.h>
#include <gst/gstutils.h>
#include <stdexcept>
#include <string>

#ifdef DEBUG
#include <iostream>
#define GST_LM_DEBUG_PRINT(...) __VA_ARGS__
#else
#define GST_LM_DEBUG_PRINT(...)
#endif

template<typename T>
struct GstTypeName
{
    static constexpr const char * value = "GstObject";
};

template<typename GstType, auto UnrefFn>
class GstLM
{
    using Self = GstLM<GstType, UnrefFn>;
    
    public:
        // Default constructor
        // Necessary for automatic class initialization in FRPipeline
        GstLM() : _el(nullptr) {}

        explicit GstLM(GstType* element) : GstLM(element, true) {}

        GstLM(GstType* element, bool throw_on_null) {
            if (!element && throw_on_null) {
                throw std::runtime_error("Failed to create " + _type_name());
            }
            this->_el = element;
        }

        GstLM(const Self& other) = delete;
        GstLM& operator=(const Self& other) = delete;

        GstLM(Self&& other) noexcept {
            this->_el = other._el;
            other._el = nullptr;
        }

        GstLM& operator=(Self&& other) {
            if (this->_el)
            {
                throw std::runtime_error("Cannot move assign " + _type_name() + ": already set!");
            }

            this->_el = other._el;
            other._el = nullptr;

            return *this;
        }

        ~GstLM() {
            if (_el) {
                GST_LM_DEBUG_PRINT(std::cout << "Unrefing " << _type_name() << ": " << GST_OBJECT_NAME(_el) << std::endl;)
                UnrefFn(_el);
            }
            else {
                GST_LM_DEBUG_PRINT(std::cout << "Skipping unrefing " << GST_OBJECT_NAME << " since it is null" << std::endl;)
            }
        }

        GstType* unpack() {
            if (!_el) {
                throw std::runtime_error(_type_name() + " is not set");
            }

            GstType* temp = _el;
            _el = nullptr;
            return temp;
        }
        
        GstType* get() const {
            if (!_el) {
                throw std::runtime_error("Trying to get a missing " + _type_name() + "! It was probably moved.");
            }
            return _el;
        }

    protected:
        static const std::string _type_name() {
            return GstTypeName<GstType>::value; 
        }
        
        GstType* _el;
};

template<>
struct GstTypeName<GstElement>
{
    static constexpr const char * value = "GstElement";
};
using GstElementLM  = GstLM<GstElement, gst_object_unref>;

template<>
struct GstTypeName<GstBus>
{
    static constexpr const char * value = "GstElement";
};
using GstBusLM      = GstLM<GstBus,     gst_object_unref>;

template<>
struct GstTypeName<GstMessage>
{
    static constexpr const char * value = "GstElement";
};
using GstMessageLM  = GstLM<GstMessage, gst_message_unref>;

template<>
struct GstTypeName<GstPad>
{
    static constexpr const char * value = "GstElement";
};
using GstPadLM      = GstLM<GstPad,     gst_object_unref>;

template<>
struct GstTypeName<GstCaps>
{
    static constexpr const char * value = "GstElement";
};
using GstCapsLM     = GstLM<GstCaps,    gst_caps_unref>;

class GstPipelineLM: public GstElementLM {
    public:
        GstPipelineLM() : GstElementLM() {}

        explicit GstPipelineLM(GstElement* pipeline) : GstElementLM(pipeline) {}

        GstPipelineLM(const GstPipelineLM& other) = delete;
        GstPipelineLM& operator=(const GstPipelineLM& other) = delete;

        GstPipelineLM(GstPipelineLM&& other) noexcept{
            this->_el = other._el;
            other._el = nullptr;
        }

        GstPipelineLM& operator=(GstPipelineLM&& other) {
            if (this->_el)
            {
                throw std::runtime_error("Cannot move assign pipeline: already set!");
            }

            this->_el = other._el;
            other._el = nullptr;

            return *this;
        }

        ~GstPipelineLM() {
            if (_el) {
                gst_element_set_state(_el, GST_STATE_NULL);
            }
        }

        GstElement* unpack() = delete;

        GstElementLM get_by_name(const std::string& name) const {
            return GstElementLM(gst_bin_get_by_name(GST_BIN(get()), name.c_str()));
        }

        void add_to_pipeline(GstElementLM& element) {
            bool res = gst_bin_add(GST_BIN(get()), element.get());

            if (res != TRUE) {
                GST_LM_DEBUG_PRINT(g_printerr("Element could not be added to pipeline.\n");)
                throw std::runtime_error("Failed to add element to pipeline");
            }
            
            element.unpack();
        }

        void link_elements(const std::string& el1, const std::string& el2) {
            GstElement* element1 = gst_bin_get_by_name(GST_BIN(get()), el1.c_str());
            GstElement* element2 = gst_bin_get_by_name(GST_BIN(get()), el2.c_str());

            if (gst_element_link(element1, element2) != TRUE) {
                GST_LM_DEBUG_PRINT(g_printerr("Elements could not be linked.\n");)
                throw std::runtime_error("Pipeline elements could not be linked");
            }
        }

        void unlink_elements(const std::string& el1, const std::string& el2) {
            GstElement* element1 = gst_bin_get_by_name(GST_BIN(get()), el1.c_str());
            GstElement* element2 = gst_bin_get_by_name(GST_BIN(get()), el2.c_str());

            gst_element_unlink(element1, element2);
        }
};

#undef GST_LM_DEBUG_PRINT
