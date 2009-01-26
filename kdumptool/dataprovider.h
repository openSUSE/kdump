/*
 * (c) 2008, Bernhard Walle <bwalle@suse.de>, SUSE LINUX Products GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#ifndef DATAPROVIDER_H
#define DATAPROVIDER_H

#include <cstdio>
#include <cstdarg>

#include "global.h"

class Progress;

//{{{ DataProvider -------------------------------------------------------------

/**
 * This interface is part of the transfer mechanism: It provides the data
 * which the Transfer class can use.
 */
class DataProvider {

    public:

        /**
         * Destructor which must be provided so that it can be overwritten
         * by subclasses.
         */
        virtual ~DataProvider() {}

        /**
         * This method is called before the DataProvider::prepare() method is
         * called the first time. This can be used to actually open the
         * connection, start the process or something like that.
         *
         * @exception KError if any error happened
         */
        virtual void prepare()
        throw (KError) = 0;

        /**
         * This method is called to check if the data provider itself is able
         * to save to a file. That makes sense for an external process that
         * accepts a file as last parameter. Saving to a file for that process
         * is much faster than using a pipe.
         *
         * @return @c true if the process is capable of saving the contents
         *            to a file, @c false otherwise.
         */
        virtual bool canSaveToFile() const
        throw () = 0;

        /**
         * If DataProvider::canSaveToFile() returns @c true, that method
         * saves the contents that the DataProvider provides to a file.
         *
         * @param[in] target the file where the contents should be saved to
         * @exception KError if saving failed or DataProvider::canSaveToFile()
         *            returns @c false.
         */
        virtual void saveToFile(const char *target)
        throw (KError) = 0;

        /**
         * This method gets called repeatedly
         *
         * @param[in] buffer the buffer where the data should be written to
         * @param[in] maxread the size of the buffer which means that maximum
         *            bytes that should be read
         * @return the number of bytes that actually have been read
         *
         * @exception KError when something goes wrong
         */
        virtual size_t getData(char *buffer, size_t maxread)
        throw (KError) = 0;

        /**
         * This method gets called after the last DataProvider::getData()
         * call. This can be used to do some cleanup, like closing the file
         * or terminating the process that provides the data.
         *
         * @exception KError if any error happened
         */
        virtual void finish()
        throw (KError) = 0;

        /**
         * Sets the error flag
         *
         * @param[in] error @c true on error, @c false on success
         */
        virtual void setError(bool error)
        throw () = 0;

        /**
         * Sets a progress notifier. The caller has to delete @p progress,
         * not this implementation.
         *
         * @param[in] progress the progress notifier.
         */
        virtual void setProgress(Progress *progress)
        throw () = 0;
};

//}}}
//{{{ AbstractDataProvider -----------------------------------------------------

/**
 * Abstract DataProvider that provides default implementations for
 * start(), stop() and the setProgress() function.
 */
class AbstractDataProvider : public DataProvider {

    public:

        /**
         * Creates a new AbstractDataProvider.
         */
        AbstractDataProvider()
        throw ();

        /**
         * Empty implementation (beside from starting the progress)
         * of DataProvider::prepare().
         */
        void prepare()
        throw (KError);

        /**
         * Empty implementation (beside from stopping the progress)
         * of DataProvider::finish().
         */
        void finish()
        throw (KError);

        /**
         * Sets the progress.
         *
         * @see DataProvider::setProgress()
         */
        void setProgress(Progress *progress)
        throw ();

        /**
         * Returns the progress.
         *
         * @return the Progress or @p NULL if not progress has been set
         */
        Progress *getProgress() const
        throw ();

        /**
         * Returns @c false as default implementation.
         *
         * @return @c false
         * @see DataProvider::canSaveToFile()
         */
        bool canSaveToFile() const
        throw ();

        /**
         * Throws a KError.
         *
         * @param[in] target the target file (does not matter)
         * @exception KError always because DataProvider::canSaveToFile()
         *            returns @c false in AbstractDataProvider.
         * @see DataProvider::saveToFile().
         */
        void saveToFile(const char *target)
        throw (KError);

        /**
         * Sets the error flag
         *
         * @param[in] error @c true on error, @c false on success
         */
        void setError(bool error)
        throw ();

        /**
         * Returns the error flag.
         *
         * @return the error flag
         */
        bool getError() const
        throw ();

    private:
        Progress *m_progress;
        bool m_error;
};

//}}}
//{{{ FileDataProvider ---------------------------------------------------------

/**
 * DataProvider that gets the data from file.
 */
class FileDataProvider : public AbstractDataProvider {

    public:

        /**
         * Creates a new FileDataProvider object.
         *
         * @param[in] filename the name of the file
         */
        FileDataProvider(const char *filename)
        throw ();

        /**
         * Actually opens the file.
         *
         * @see DataProvider::prepare()
         */
        void prepare()
        throw (KError);

        /**
         * Provides the data.
         *
         * @see DataProvider::getData()
         */
        size_t getData(char *buffer, size_t maxread)
        throw (KError);

        /**
         * Closes the file.
         *
         * @see DataProvider::finish()
         */
        virtual void finish()
        throw (KError);

    private:
        std::string m_filename;
        FILE *m_file;
        loff_t m_fileSize;
        loff_t m_currentPos;
};

//}}}
//{{{ BufferDataProvider -------------------------------------------------------

/**
 * BufferDataProvider that gets the data from file.
 */
class BufferDataProvider : public AbstractDataProvider {

    public:

        /**
         * Creates a new BufferDataProvider object.
         *
         * @param[in] data the buffer
         */
        BufferDataProvider(const ByteVector &data)
        throw ();

        /**
         * Provides the data.
         *
         * @see DataProvider::getData()
         */
        size_t getData(char *buffer, size_t maxread)
        throw (KError);

    private:
        ByteVector m_data;
        unsigned long long m_currentPos;
};

//}}}
//{{{ ProcessDataProvider ------------------------------------------------------

/**
 * ProcessDataProvider is a DataProvider that gets the data from stdout from
 * a process. It does not make sense to set a Progress notifier for that type
 * of DataProvider because we don't know when the data stream ends.
 */
class ProcessDataProvider : public AbstractDataProvider {

    public:

        /**
         * Creates a new BufferDataProvider object.
         *
         * @param[in] data the buffer
         * @param[in] add_cmdline additional parameters when the
         *            ProcessDataProvider::saveToFile() shortcut is used
         */
        ProcessDataProvider(const char *cmdline, const char *add_cmdline="")
        throw ();

        /**
         * Returns @c true.
         *
         * @return @c true
         */
        bool canSaveToFile() const
        throw ();

        /**
         * Runs the process with @c target as last parameter to save the
         * stuff to a file directly.
         *
         * @param[in] target the target file
         * @param KError if saving to the file failed
         */
        void saveToFile(const char *target)
        throw (KError);

        /**
         * Starts the process
         *
         * @see DataProvider::prepare()
         */
        virtual void prepare()
        throw (KError);

        /**
         * Provides the data.
         *
         * @see DataProvider::getData()
         */
        size_t getData(char *buffer, size_t maxread)
        throw (KError);

        /**
         * Terminates the process.
         *
         * @see DataProvider::finish()
         */
        virtual void finish()
        throw (KError);

    private:
        std::string m_pipeCmdline;
        std::string m_directCmdline;
        FILE *m_processFile;
};

//}}}


#endif /* DATAPROVIDER_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
